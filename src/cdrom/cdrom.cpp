#include "cdrom.hpp"

#include "bus/bus.hpp"
#include "fmt/format.h"
#include "magic_enum.hpp"
#include "scheduler/scheduler.hpp"
#include "support/log.hpp"

namespace CDROM {

CDROM::CDROM(Scheduler::Scheduler& scheduler) : scheduler(scheduler) {
    reset();
    m_discLoaded = false;
}

void CDROM::loadDisc(const std::filesystem::path& path) {
    if (m_discImage.is_open()) m_discImage.close();
    m_discImage.open(path, std::ios::binary | std::ios::in);
    if (!m_discImage.is_open()) {
        Log::warn("Failed to open disc image: {}", path.string());
        m_discLoaded = false;
    }
    m_discImage.unsetf(std::ios::skipws);
    m_discLoaded = true;
}

void CDROM::reset() {
    m_status.r = 0;
    m_status.ParamFifoEmpty = 1;
    m_status.ParamFifoWriteReady = 1;
    m_statusCode.r = 0;
    m_command = Commands::None;
    m_pendingCommand = Commands::None;
    if (m_discLoaded) m_discImage.seekg(std::ios::beg);
    m_sector.resize(2352);
    m_dataFifoIndex = 0;
}

void CDROM::newCommand(u8 value) {
    using enum Commands;

    m_status.Busy = 1;
    m_pendingCommand = static_cast<Commands>(value);
    tryStartCommand();
}

void CDROM::scheduleInterrupt(u32 cycles) {
    scheduler.scheduleEvent(cycles, [&]() {
        m_irqFlags |= m_ints.front();
        scheduler.bus.triggerInterrupt(Bus::IRQ::CDROM);
        m_ints.pop();
    });
}

void CDROM::scheduleCommandFinish(u32 cycles) {
    scheduler.scheduleEvent(cycles, [this]() { tryFinishCommand(); });
}

void CDROM::scheduleStartCommand(u32 cycles) {
    scheduler.scheduleEvent(cycles, [this]() { tryStartCommand(); });
}

void CDROM::scheduleRead() {
    u32 speed = m_mode.Speed ? 150 : 75;
    u32 cycles = 33868800 / speed;
    if (m_delayFirstRead) {
        cycles = 33868800 * 3;
        m_delayFirstRead = false;
    }
    scheduler.scheduleEvent(cycles, [this]() { readSector(); });
}

void CDROM::readSector() {
    if (m_state != State::Read) return;
    m_discImage.read(reinterpret_cast<char*>(m_sector.data()), 2352);
    m_responseFifo.push(m_statusCode.r);
    m_ints.emplace(InterruptCause::INT1);
    scheduleInterrupt(1);
    scheduleRead();
}

void CDROM::tryStartCommand() {
    using enum Commands;

    //    if (m_irqFlags & 0x7 || m_irqFlags & 0x1F) return;
    if (!m_responseFifo.empty()) {
        scheduleStartCommand(500);
        return;
    }
    if (m_pendingCommand == None) return;
    m_command = m_pendingCommand;
    m_pendingCommand = None;

    Log::debug("[CDROM] Starting Command: {}\n", magic_enum::enum_name(m_command));

    if (m_command == Init) {
        m_statusCode.r = 0;
        m_statusCode.Motor = 1;
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT3);
        scheduleInterrupt(120000);
        scheduleCommandFinish(durationToCycles(std::chrono::milliseconds(750)));
    }

    if (m_command == SetMode) {
        auto oldMode = m_mode;
        m_mode.r = m_paramFifo.front();
        if (oldMode.Speed.Value() == 0 && m_mode.Speed.Value() == 1) {
            m_delayFirstRead = true;
        }
        m_paramFifo.pop();
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT3);
        scheduleInterrupt(120000);
    }

    if (m_command == GetID) {
        if (m_trayOpen) {
            for (auto v : c_trayOpen) {
                m_responseFifo.push(v);
            }
            m_ints.emplace(InterruptCause::INT5);
            scheduleInterrupt(120000);
        } else
            m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT3);
        scheduleInterrupt(120000);
        scheduleCommandFinish(125000);
    }

    if (m_command == Test) {
        if (m_paramFifo.front() == 0x20) {
            m_paramFifo.pop();
            for (auto v : c_version) {
                m_responseFifo.push(v);
                m_ints.emplace(InterruptCause::INT3);
                scheduleInterrupt(120000);
            }
        }
    }

    if (m_command == GetStat) {
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT3);
        scheduleInterrupt(120000);
        if (m_trayChanged) {
            if (m_trayOpen) {
                bool oldError = m_statusCode.Error.Value();
                m_statusCode.r = 0;
                m_statusCode.Error = oldError;
            }
            m_statusCode.ShellOpen = m_trayOpen;
            m_trayChanged = false;
        }
    }

    if (m_command == ReadTOC) {
        m_statusCode.Motor = 1;
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT3);
        scheduleInterrupt(120000);
        m_statusCode.Read = 1;
        scheduleCommandFinish(33868800);
    }

    if (m_command == SetLoc) {
        m_location.setM(m_paramFifo.front());
        m_paramFifo.pop();
        m_location.setS(m_paramFifo.front());
        m_paramFifo.pop();
        m_location.setF(m_paramFifo.front());
        m_paramFifo.pop();
        m_setlocLSN = m_location.toLSN() * 2352;
        m_updateLoc = true;
        m_ints.emplace(InterruptCause::INT3);
        m_responseFifo.push(m_statusCode.r);
        scheduleInterrupt(120000);
//        Log::info("[CDROM] SetLoc to {}:{}:{} to LSN: {}\n", m_location.min, m_location.sec, m_location.sect, m_setlocLSN);
    }

    if (m_command == SeekL) {
        m_lsn = m_setlocLSN;
        m_state = State::Idle;
        m_updateLoc = false;
        m_statusCode.r = 0;
        m_statusCode.Motor = 1;
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT3);
        scheduleInterrupt(120000);
        scheduleCommandFinish(125000);
        m_discImage.seekg(m_lsn, std::ios::beg);
    }

    if (m_command == ReadN) {
        m_state = State::Read;
        m_responseFifo.push(m_statusCode.r);
        m_statusCode.Read = 1;
        m_ints.emplace(InterruptCause::INT3);
        scheduleInterrupt(120000);
        if (m_updateLoc) {
            m_lsn = m_setlocLSN;
            m_discImage.seekg(m_lsn, std::ios::beg);
            m_updateLoc = false;
//            Log::info("[CDROM] Implicit Seek to LSN: {}\n", m_lsn);
        }
        scheduleRead();
    }

    if (m_command == Pause) {
        m_state = State::Idle;
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT3);
        scheduleInterrupt(120000);
        scheduleCommandFinish(durationToCycles(std::chrono::milliseconds(350)));
    }

    if (m_command == Demute) {
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT3);
        scheduleInterrupt(130000);
    }
}

void CDROM::tryFinishCommand() {
    using enum Commands;

    if (!m_responseFifo.empty()) {
        scheduleCommandFinish(500);
        return;
    }

    if (m_ints.size() > 1) {
        scheduleCommandFinish(500);
        return;
    }

    if (m_command == Init) {
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT2);
        scheduleInterrupt(durationToCycles(std::chrono::microseconds(3000)));
    }

    if (m_command == GetID) {
        if (!m_discLoaded) {
            for (auto v : c_noDisk) {
                m_responseFifo.push(v);
            }
            m_ints.emplace(InterruptCause::INT5);
            scheduleInterrupt(10000);
        } else {
            m_responseFifo.push(m_statusCode.r);
            for (auto v : c_licMode2) {
                m_responseFifo.push(v);
            }
            m_ints.emplace(InterruptCause::INT2);
            scheduleInterrupt(5000);
        }
    }

    if (m_command == ReadTOC) {
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT2);
        scheduleInterrupt(1000);
        m_statusCode.Read = 0;
    }

    if (m_command == SeekL) {
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT2);
        scheduleInterrupt(2000);
    }

    if (m_command == Pause) {
        m_statusCode.r = 0;
        m_statusCode.Motor = 1;
        m_responseFifo.push(m_statusCode.r);
        m_ints.emplace(InterruptCause::INT2);
        scheduleInterrupt(50000);
    }

    m_command = None;
}

u8 CDROM::read(u32 offset) {
    switch (offset) {
        case 0: return read0();
        case 1: return read1();
        case 2: return read2();
        case 3: return read3();

        default: Log::warn("CDROM::read() invalid offset: {}\n", offset); return 0;
    }
}

u8 CDROM::read0() {
    // Read from status register
    if (m_responseFifo.empty()) {
        m_status.ResponseFifoReadReady = 0;
    } else {
        m_status.ResponseFifoReadReady = 1;
    }

    if (m_paramFifo.empty()) {
        m_status.ParamFifoEmpty = 1;
    } else {
        m_status.ParamFifoEmpty = 0;
    }

    if (m_paramFifo.size() >= 16) {
        m_status.ParamFifoWriteReady = 0;
    } else {
        m_status.ParamFifoWriteReady = 1;
    }

    //    u32 m_temp = m_mode.SectorSize.Value() ? 2340 : 2048;
    //    if (m_dataFifo.empty() || m_dataFifoIndex == m_temp) {
    //        m_status.DataFifoReadReady = 0;
    //    }

    // DataFifo Status

    return m_status.r;
}

u8 CDROM::read1() {
    // Read from response FIFO
    auto val = m_responseFifo.front();
    m_responseFifo.pop();
    if (m_responseFifo.empty()) {
        m_status.ResponseFifoReadReady = 0;
    }
    return val;
}

u8 CDROM::read2() {
    // Read from data FIFO
    if (m_dataFifo.empty()) return 0;
    u32 sectorSize = m_mode.SectorSize.Value() ? 2340 : 2048;
    u32 data = sectorSize == 2340 ? 12 : 24;

    u8 val = m_dataFifo[m_dataFifoIndex + data];
    m_dataFifoIndex++;
    //    Log<Log::CDROM>("Data Fifo Index: {}\n", m_dataFifoIndex);

    if (m_dataFifo.empty() || m_dataFifoIndex == sectorSize) {
        m_status.DataFifoReadReady = 0;
    }

    return val;
}

u8 CDROM::read3() {
    // Index 0 - Interrupt Enable
    // Index 1 - Interrupt Flags
    // Index 2 - Interrupt Enable Mirror
    // Index 3 - Interrupt Flags Mirror
    if (m_status.Index.Value() % 2 == 0) {
        return m_irqEnable | 0xE0;
    } else {
        return m_irqFlags | 0xE0;
    }
}

void CDROM::write(u32 offset, u8 value) {
    switch (offset) {
        case 0: return write0(value);
        case 1: return write1(value);
        case 2: return write2(value);
        case 3: return write3(value);

        default: Log::warn("CDROM::write() invalid offset\n"); return;
    }
}

void CDROM::write0(u8 value) {
    // Write to index register
    m_status.Index = value & 3;
}

void CDROM::write1(u8 value) {
    // Index 0 - Write to command register
    // Index 1 - Write to Sound Map Data Out
    // Index 2 - Write to Sound Map Coding Info
    // Index 3 - Write to Right-CD to Right-SPU Volume
    switch (m_status.Index.Value()) {
        case 0: newCommand(value); break;
        case 1:
        case 2:
        case 3: break;
    }
}

void CDROM::write2(u8 value) {
    // Index 0 - Write to parameter FIFO
    // Index 1 - Write to Interrupt Enable Register
    // Index 2 - Write to Left-CD to Left-SPU Volume
    // Index 3 - Write to Right-CD to Left-SPU Volume
    switch (m_status.Index.Value()) {
        case 0: m_paramFifo.push(value); break;
        case 1: m_irqEnable = value & 0x1F; break;
        case 2:
        case 3: break;
    }
}

void CDROM::write3(u8 value) {
    // Index 0 - Write to Interrupt Request Register
    // Index 1 - Write to Interrupt Flags Register
    // Index 2 - Write to Left-CD to Right-SPU Volume
    // Index 3 - Write to Audio Volume Apply Changes
    switch (m_status.Index.Value()) {
        case 0: {
            m_request.r = value;
            if (value & 0x80) {
                Log::debug("[CDROM] Request Register: Data Requested\n");
                u32 sectorSize = (m_mode.SectorSize.Value() == 1 ? 2340 : 2048);
                if (m_dataFifo.empty() || m_dataFifoIndex >= sectorSize) {
                    m_dataFifoIndex = 0;
                    m_status.DataFifoReadReady = 1;
                    m_dataFifo = m_sector;
                }
            } else {
                Log::debug("[CDROM] Request Register: Data Not Requested - Clearing data Fifo\n");
                m_dataFifo.clear();
                m_dataFifoIndex = 0;
                m_status.DataFifoReadReady = 0;
            }
        }
        case 1: {
            Log::debug(
                "[CDROM] Acking IRQ Flags with {:#02x} - previous: {:#02x} new: {:#02x}\n", value & 0x1F, m_irqFlags & 0x1F,
                m_irqFlags & ~(value & 0x1F)
            );
            m_irqFlags &= ~(value & 0x1F);
            m_status.Busy = 0;
            if (value & 0x40) {
                // Clear Param Fifo
                clearParamFifo();
            }
        }
        case 2:
        case 3: break;
    }
}

u32 CDROM::dmaRead() {
    u32 value = 0;
    value |= read2() << 0;
    value |= read2() << 8;
    value |= read2() << 16;
    value |= read2() << 24;
    return value;
}

}  // namespace CDROM

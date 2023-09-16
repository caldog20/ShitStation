#include "sio.hpp"

#include "bus/bus.hpp"

namespace SIO {

SIO::SIO(Scheduler::Scheduler& scheduler) : pad(*this), m_scheduler(scheduler) { reset(); }

void SIO::reset() {
    m_regs.control.r = 0;
    m_regs.baud = 0;
    m_regs.mode.r = 0;  // 13
    m_regs.txData = 0;
    m_regs.rxData = 0;
    m_regs.stat.r = 0;
    m_regs.stat.AckInputLevel = 1;
    m_regs.stat.TXReady1 = 1;
    m_regs.stat.TXReady2 = 1;
    m_regs.stat.FifoNotEmpty = 0;
    m_fifo.clear();
}

void SIO::txData(u8 data) {
    m_regs.txData = data;
    m_regs.stat.TXReady1 = 0;
    if (!m_regs.control.TXEN) return;
    if (m_regs.stat.TXReady2 && !(m_regs.stat.TXReady1)) {
        startTransfer();
    }
}

void SIO::startTransfer() {
    using enum DeviceType;

    m_regs.stat.TXReady2 = 0;

    if (m_deviceType == None) {
        m_deviceType = static_cast<DeviceType>(m_regs.txData);
    }

    if (m_deviceType == Pad) {
        pad.transfer(m_regs.txData);
    } else if (m_deviceType == MemoryCard) {
        m_fifo.push(0xff);
    }

    setFifoStatus();

    // Receive Byte size IRQ

    m_regs.stat.TXReady1 = 1;
    m_regs.stat.TXReady2 = 1;
}

void SIO::rxData() {
    m_regs.rxData = 0xFF;
    if (m_regs.stat.FifoNotEmpty && m_fifo.size() > 0) {
        m_regs.rxData = m_fifo.pop();
        setFifoStatus();
    }
}

void SIO::setFifoStatus() {
    if (m_fifo.size() > 0) {
        m_regs.stat.FifoNotEmpty = 1;
    } else {
        m_regs.stat.FifoNotEmpty = 0;
    }
}

void SIO::writeControl(u16 value) {
    const bool deselected = (m_regs.control.JoyNOutput) && (!(value & (1 << 1)));
    const bool selected = (!(m_regs.control.JoyNOutput)) && (value & (1 << 1));
    const bool changed = (m_regs.control.Slot) && (!(value & (1 << 13)));

    m_regs.control.r = value;

    if (deselected || changed) {
        m_deviceType = DeviceType::None;
        pad.m_status = Pad::Status::Idle;
        m_fifo.clear();
    }

    if (m_regs.control.Ack) {
        m_regs.stat.RXParityError = 0;
        m_regs.stat.IRQ = 0;
    }

    if (m_regs.control.Reset) {
        reset();
        pad.m_status = Pad::Status::Idle;
        m_deviceType = DeviceType::None;
    }

    setFifoStatus();

    if (m_regs.control.TXInterruptEnable) {
        m_regs.stat.IRQ = 1;
    }
}

void SIO::ack(bool ack) {
    if (ack) {
        m_regs.stat.AckInputLevel = 0;  // 0 is ACK, 1 is NO ACK
        if (m_regs.control.AckInterruptEnable && m_regs.stat.IRQ == 0) {
            m_regs.stat.IRQ = 1;
            scheduleIRQ();
            m_regs.stat.AckInputLevel = 1;
        }
    } else {
        m_regs.stat.AckInputLevel = 1;
    }
}

void SIO::scheduleIRQ() {
    m_scheduler.scheduleEvent(1000, [&] {
        m_regs.stat.IRQ = 0;
        m_scheduler.bus.triggerInterrupt(Bus::IRQ::PAD);
        setFifoStatus();
    });
}

template <typename T>
T SIO::read(u32 offset) {
    switch (offset) {
        case 0x0: {
            rxData();
            // Log<Log::SIO>("SIO: Read RX DATA {:#x}\n", m_regs.rxData);
            return m_regs.rxData;
        }
        case 0x4: {
            // Log<Log::SIO>("SIO: Read Stat {:#x}\n", m_regs.stat.r);
            return (T)m_regs.stat.r;
        }
        case 0x8: {
            // Log<Log::SIO>("SIO: Read Mode {:#x}\n", m_regs.mode.r);
            return (T)m_regs.mode.r;
        }
        case 0xA: {
            // Log<Log::SIO>("SIO: Read Control {:#x}\n", m_regs.control.r);
            return (T)m_regs.control.r;
        }
        case 0xE: {
            // Log<Log::SIO>("SIO: Read Baud {:#x}\n", m_regs.baud);
            return (T)m_regs.baud;
        }
    }
}

template <typename T>
void SIO::write(u32 offset, T value) {
    switch (offset) {
        case 0x0: {
            // Log<Log::SIO>("SIO: Write txData {:#x}\n", value);
            txData(value);
            break;
        }
        case 0x4: {
            // Stat reg Read Only
            break;
        }
        case 0x8: {
            // Log<Log::SIO>("SIO: Write Mode {:#x}\n", value);
            m_regs.mode.r = (u16)value;
            break;
        }
        case 0xA: {
            // Log<Log::SIO>("SIO: Write Control {:#x}\n", value);
            writeControl(value);
            return;
        }
        case 0xE: {
            // Log<Log::SIO>("SIO: Write Baud {:#x}\n", value);
            m_regs.baud = (u16)value;
            m_regs.stat.BaudTimer = (u32)(m_regs.baud * m_regs.mode.BaudReloadFactor) & ~0x1;
            break;
        }
    }
}

template u8 SIO::read<u8>(u32);
template u16 SIO::read<u16>(u32);
template u32 SIO::read<u32>(u32);

template void SIO::write<u8>(u32, u8);
template void SIO::write<u16>(u32, u16);
template void SIO::write<u32>(u32, u32);

}  // namespace SIO

Pad::Pad(SIO::SIO& sio) : sio(sio) { reset(); }

void Pad::keyCallback(SDL_KeyboardEvent& event) {
    auto button = m_buttonLUT[event.keysym.sym];
    if (button == 0) return;
    if (event.type == SDL_KEYDOWN) {
        m_buttons &= ~(button);
    } else {
        m_buttons |= button;
    }
}

void Pad::setIdle() { m_status = Status::Idle; }

void Pad::transfer(u8 data) {
    switch (m_status) {
        case Status::Idle: {
            if (data == Commands::Initialize) {
                m_status = Status::Connected;
                sio.m_fifo.push(0xFF);
                sio.ack(true);
                return;
            } else {  // Not Connected
                sio.m_fifo.clear();
                sio.ack(false);
                sio.m_fifo.push(0xFF);
                return;
            }
        }

        case Status::Connected: {
            if (data == Commands::Read) {
                m_status = Status::Transferring;
                sio.m_fifo.push(static_cast<u16>(m_type) & 0xFF);
                sio.m_fifo.push((static_cast<u16>(m_type) >> 8) & 0xFF);
                sio.m_fifo.push(m_buttons & 0xFF);
                sio.m_fifo.push((m_buttons >> 8) & 0xFF);

                sio.ack(true);
                return;
            } else {
                setIdle();
                sio.m_fifo.clear();
                sio.ack(false);
                sio.m_fifo.push(0xFF);
                return;
            }
        }
        case Status::Transferring: {
            if (sio.m_fifo.size() > 0) {
                sio.ack(true);
            } else {
                sio.ack(false);
                setIdle();
            }
        }
        default: {
            return;
        }
    }
}

void Pad::init() {
    m_status = Status::Idle;
    m_type = ControllerType::Digital;
    initKeyCodes();
    m_buttons = 0xffff;
}

void Pad::initKeyCodes() {
    m_buttonLUT[SDLK_UP] = DigitalPadInputs::Up;
    m_buttonLUT[SDLK_DOWN] = DigitalPadInputs::Down;
    m_buttonLUT[SDLK_LEFT] = DigitalPadInputs::Left;
    m_buttonLUT[SDLK_RIGHT] = DigitalPadInputs::Right;

    m_buttonLUT[SDLK_w] = DigitalPadInputs::Triangle;
    m_buttonLUT[SDLK_a] = DigitalPadInputs::Square;
    m_buttonLUT[SDLK_s] = DigitalPadInputs::Cross;
    m_buttonLUT[SDLK_d] = DigitalPadInputs::Circle;

    m_buttonLUT[SDLK_q] = DigitalPadInputs::L1;
    m_buttonLUT[SDLK_e] = DigitalPadInputs::L2;
    m_buttonLUT[SDLK_1] = DigitalPadInputs::R1;
    m_buttonLUT[SDLK_3] = DigitalPadInputs::R2;

    m_buttonLUT[SDLK_RETURN] = DigitalPadInputs::Start;

    // Do Other keys
}

// TODO Replace hardcoded values when more pads are implemented
void Pad::reset() {
    m_status = Status::Idle;
    m_type = ControllerType::Digital;
    m_buttons = 0xffff;
    initKeyCodes();
}

void Pad::ack() {}

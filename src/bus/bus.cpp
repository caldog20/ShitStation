#include "bus.hpp"

#include "cpu/cpu.hpp"
#include "dma/dmacontroller.hpp"
#include "support/log.hpp"
#include "timers/timers.hpp"

namespace Bus {

Bus::Bus(Cpu::Cpu& cpu, DMA::DMA& dma, Timers::Timers& timers) : cpu(cpu), dma(dma), timers(timers) {
    try {
        ram = new u8[MemorySize::Ram];
        bios = new u8[MemorySize::Bios];
        scratchpad = new u8[MemorySize::Scratchpad];
        readPages = new uintptr_t[MemorySize::FastMem];
        writePages = new uintptr_t[MemorySize::FastMem];
    } catch (...) {
        Helpers::panic("[BUS] Failed to allocate Emulator memory\n");
    }

    constexpr u32 PAGE_SIZE = 64_KB;

    // RAM Read Pages
    for (auto index = 0; index < 128; index++) {
        const auto pointer = (uintptr_t)&ram[index * PAGE_SIZE];
        readPages[index + 0x0000] = pointer;  // KUSEG
        readPages[index + 0x8000] = pointer;  // KSEG0
        readPages[index + 0xA000] = pointer;  // KSEG1
    }
    // RAM Write Pages
    for (auto index = 0; index < 128; index++) {
        const auto pointer = (uintptr_t)&ram[index * PAGE_SIZE];
        writePages[index + 0x0000] = pointer;  // KUSEG
        writePages[index + 0x8000] = pointer;  // KSEG0
        writePages[index + 0xA000] = pointer;  // KSEG1
    }
    // BIOS Read Pages
    for (auto index = 0; index < 8; index++) {
        const auto pointer = (uintptr_t)&bios[index * PAGE_SIZE];
        readPages[index + 0x1FC0] = pointer;  // KUSEG BIOS
        readPages[index + 0x9FC0] = pointer;  // KSEG0 BIOS
        readPages[index + 0xBFC0] = pointer;  // KSEG1 BIOS
    }
}

Bus::~Bus() {
    delete[] ram;
    delete[] bios;
    delete[] scratchpad;
    delete[] readPages;
    delete[] writePages;
}

void Bus::reset() {
    std::memset(ram, 0, MemorySize::Ram);
    std::memset(scratchpad, 0, MemorySize::Scratchpad);
    std::memset(MemControl, 0, sizeof(MemControl));
    MemControl2 = 0;
    CacheControl = 0;
    ISTAT = IMASK = 0;

    // temporary
    std::memset(spu, 0, sizeof(spu));
}

u32 Bus::fetch(u32 address) {
    const auto page = address >> 16;
    const auto offset = address & 0xFFFF;
    const auto pointer = readPages[page];

    if (page == 0xBFC0 || page == 0x9FC0 || page == 0x1FC0) {
        cpu.addCycles(CycleBias::ROM);
    }

    // BIOS/RAM Fastmem Reads
    if (pointer != 0) {
        return *(u32*)(pointer + offset);
    }
}

u8 Bus::read8(u32 address) {
    const auto page = address >> 16;
    const auto offset = address & 0xFFFF;
    const auto pointer = readPages[page];

    if (cpu.isCacheIsolated()) return 0;

    cpu.addCycles(CycleBias::RAM);

    if (page == 0xBFC0 || page == 0x9FC0 || page == 0x1FC0) {
        cpu.addCycles(CycleBias::ROM);
    }

    // BIOS/RAM Fastmem Reads
    if (pointer != 0) {
        return *(u8*)(pointer + offset);
    }

    // Slow Reads to Scratchpad
    if ((page == 0x1F80 || page == 0x9F80) && offset < MemorySize::Scratchpad) {
        return *(u8*)(scratchpad + offset);
    } else if (page == 0xBF80 && offset < MemorySize::Scratchpad) {
        Helpers::panic("[BUS] u8 scratchpad read from KSEG1 prohibited\n");
    }

    // Slow Reads to MMIO
    auto hw_address = mask(address);

    // PAD
    // EXP1
    // EXP2
    // CDROM
    // DMA
    // SIO

    Log::warn("[BUS] [READ8] Unhandled read at address: {:08x}\n", address);
    return 0;
}

u16 Bus::read16(u32 address) {
    const auto page = address >> 16;
    const auto offset = address & 0xFFFF;
    const auto pointer = readPages[page];

    if (cpu.isCacheIsolated()) return 0;

    cpu.addCycles(CycleBias::RAM);

    if (page == 0xBFC0 || page == 0x9FC0 || page == 0x1FC0) {
        cpu.addCycles(CycleBias::ROM);
    }

    // BIOS/RAM Fastmem Reads
    if (pointer != 0) {
        return *(u16*)(pointer + offset);
    }

    // Slow Reads to Scratchpad
    if ((page == 0x1F80 || page == 0x9F80) && offset < MemorySize::Scratchpad) {
        return *(u16*)(scratchpad + offset);
    } else if (page == 0xBF80 && offset < MemorySize::Scratchpad) {
        Helpers::panic("[BUS] u8 scratchpad read from KSEG1 prohibited\n");
    }

    // Slow Reads to MMIO
    auto hw_address = mask(address);

    if (IRQCONTROL.contains(hw_address)) {
        auto offset = IRQCONTROL.offset(hw_address);
        if (offset == 0) {
            return ISTAT;
        } else if (offset == 4) {
            return IMASK;
        }
    }

    if (SPU.contains(hw_address)) {
        auto offset = SPU.offset(hw_address);
        return *(u16*)(spu + offset);
    }

    if (TIMERS.contains(hw_address)) {
        auto offset = TIMERS.offset(hw_address);
        return timers.read(offset);
        return 0;
    }

    // PAD
    // SIO
    // EXP1

    Log::warn("[BUS] [READ16] Unhandled read at address: {:08x}\n", address);
    return 0;
}

u32 Bus::read32(u32 address) {
    const auto page = address >> 16;
    const auto offset = address & 0xFFFF;
    const auto pointer = readPages[page];

    if (cpu.isCacheIsolated()) return 0;

    cpu.addCycles(CycleBias::RAM);

    if (page == 0xBFC0 || page == 0x9FC0 || page == 0x1FC0) {
        cpu.addCycles(CycleBias::ROM);
    }

    // BIOS/RAM Fastmem Reads
    if (pointer != 0) {
        return *(u32*)(pointer + offset);
    }

    // Slow Reads to Scratchpad
    if ((page == 0x1F80 || page == 0x9F80) && offset < MemorySize::Scratchpad) {
        return *(u32*)(scratchpad + offset);
    } else if (page == 0xBF80 && offset < MemorySize::Scratchpad) {
        Helpers::panic("[BUS] u8 scratchpad read from KSEG1 prohibited\n");
    }

    // Slow Reads to MMIO
    auto hw_address = mask(address);

    if (GPU.contains(hw_address)) {
        auto offset = GPU.offset(hw_address);
        if (offset == 0) {
            return 0;
        }
        if (offset == 4) {
            return 0b01011110100000000000000000000000;
        }
    }

    if (IRQCONTROL.contains(hw_address)) {
        auto offset = IRQCONTROL.offset(hw_address);
        if (offset == 0) {
            return ISTAT;
        } else if (offset == 4) {
            return IMASK;
        }
    }

    if (DMA.contains(hw_address)) {
        auto offset = DMA.offset(hw_address);
        return dma.read(offset);
    }

    if (TIMERS.contains(hw_address)) {
        auto offset = TIMERS.offset(hw_address);
        return timers.read(offset);
        return 0;
    }

    // EXP1
    // GPU

    Log::warn("[BUS] [READ32] Unhandled read at address: {:08x}\n", address);
    return 0;
}

void Bus::write8(u32 address, u8 value) {
    const auto page = address >> 16;
    const auto offset = address & 0xFFFF;
    const auto pointer = writePages[page];

    if (cpu.isCacheIsolated()) return;

    cpu.addCycles(CycleBias::RAM);

    if (page == 0xBFC0 || page == 0x9FC0 || page == 0x1FC0) {
        cpu.addCycles(CycleBias::ROM);
    }

    // BIOS/RAM Fastmem Writes
    if (pointer != 0) {
        *(u8*)(pointer + offset) = value;
        return;
    }

    // Slow writes to Scratchpad
    if ((page == 0x1F80 || page == 0x9F80) && offset < MemorySize::Scratchpad) {
        *(u8*)(scratchpad + offset) = value;
        return;
    } else if (page == 0xBF80 && offset < MemorySize::Scratchpad) {
        Helpers::panic("[BUS] u8 scratchpad write from KSEG1 prohibited\n");
    }

    // Slow writes to MMIO
    auto hw_address = mask(address);

    if (DMA.contains(hw_address)) {
        auto offset = DMA.offset(hw_address);
        return dma.write8(offset, value);
    }

    // PAD
    // EXP2
    // CDROM
    // DMA ???
    Log::warn("[BUS] [WRITE8] Unhandled write at address: {:08x} : value: {:08x}\n", address, value);
}

void Bus::write16(u32 address, u16 value) {
    const auto page = address >> 16;
    const auto offset = address & 0xFFFF;
    const auto pointer = writePages[page];

    if (cpu.isCacheIsolated()) return;

    cpu.addCycles(CycleBias::RAM);

    if (page == 0xBFC0 || page == 0x9FC0 || page == 0x1FC0) {
        cpu.addCycles(CycleBias::ROM);
    }

    // BIOS/RAM Fastmem Writes
    if (pointer != 0) {
        *(u16*)(pointer + offset) = value;
        return;
    }

    // Slow writes to Scratchpad
    if ((page == 0x1F80 || page == 0x9F80) && offset < MemorySize::Scratchpad) {
        *(u16*)(scratchpad + offset) = value;
        return;
    } else if (page == 0xBF80 && offset < MemorySize::Scratchpad) {
        Helpers::panic("[BUS] u16 scratchpad write from KSEG1 prohibited\n");
    }

    // Slow writes to MMIO
    auto hw_address = mask(address);

    if (IRQCONTROL.contains(hw_address)) {
        auto offset = IRQCONTROL.offset(hw_address);
        if (offset == 0) {
            ISTAT &= (value & 0x7FF);
        } else if (offset == 4) {
            IMASK = (value & 0x7FF);
        }
        return;
    }

    if (SPU.contains(hw_address)) {
        auto offset = SPU.offset(hw_address);
        *(u16*)(spu + offset) = value;
        return;
    }

    if (TIMERS.contains(hw_address)) {
        auto offset = TIMERS.offset(hw_address);
        timers.write(offset, value);
        return;
    }

    // SPU
    // PAD
    // SIO
    Log::warn("[BUS] [WRITE16] Unhandled write at address: {:08x} : value: {:08x}\n", address, value);
}

void Bus::write32(u32 address, u32 value) {
    const auto page = address >> 16;
    const auto offset = address & 0xFFFF;
    const auto pointer = writePages[page];

    if (cpu.isCacheIsolated()) return;

    cpu.addCycles(CycleBias::RAM);

    if (page == 0xBFC0 || page == 0x9FC0 || page == 0x1FC0) {
        cpu.addCycles(CycleBias::ROM);
    }

    // BIOS/RAM Fastmem Writes
    if (pointer != 0) {
        *(u32*)(pointer + offset) = value;
        return;
    }

    // Slow writes to Scratchpad
    if ((page == 0x1F80 || page == 0x9F80) && offset < MemorySize::Scratchpad) {
        *(u32*)(scratchpad + offset) = value;
        return;
    } else if (page == 0xBF80 && offset < MemorySize::Scratchpad) {
        Helpers::panic("[BUS] u32 scratchpad write from KSEG1 prohibited\n");
    }

    // Slow writes to MMIO
    auto hw_address = mask(address);

    if (GPU.contains(hw_address)) {
        return;
    }

    if (IRQCONTROL.contains(hw_address)) {
        auto offset = IRQCONTROL.offset(hw_address);
        if (offset == 0) {
            ISTAT &= (value & 0x7FF);
        } else if (offset == 4) {
            IMASK = (value & 0x7FF);
        }
        return;
    }

    if (MEMCONTROL.contains(hw_address)) {
        auto offset = MEMCONTROL.offset(hw_address);
        MemControl[offset >> 2] = value;
        return;
    }

    if (MEMCONTROL2.contains(hw_address)) {
        MemControl2 = value;
        return;
    }

    if (CACHECONTROL.contains(hw_address)) {
        CacheControl = value;
        return;
    }

    if (DMA.contains(hw_address)) {
        auto offset = DMA.offset(hw_address);
        return dma.write(offset, value);
    }

    if (TIMERS.contains(hw_address)) {
        auto offset = TIMERS.offset(hw_address);
        timers.write(offset, value);
        return;
    }

    // SPU
    // GPU
    Log::warn("[BUS] [WRITE32] Unhandled write at address: {:08x} : value: {:08x}\n", address, value);
}

void Bus::triggerInterrupt(IRQ irq) {
    ISTAT |= (1 << static_cast<u16>(irq));
    cpu.triggerInterrupt();
}

void Bus::doSideload() {
    auto addr = mask(sideloadAddr);
    auto size = sideloadEXE.size();
    cpu.setPC(sideloadPC);
    for (auto i = 0; i < size; i++) {
        ram[addr + i] = sideloadEXE[i];
    }
}

}  // namespace Bus

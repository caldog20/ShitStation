#include "bus.hpp"

#include "cpu/cpu.hpp"
#include "support/log.hpp"

namespace Bus {

Bus::Bus(Cpu::Cpu& cpu) : cpu(cpu) {
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

    // IRQ
    // SPU
    // PAD
    // SIO
    // EXP1
    // TIMERS

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

    // IRQ
    // DMA
    // TIMERS
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

    // TIMERS
    // SPU
    // IRQ
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

    // SPU
    // IRQ
    // MEMCONTROL
    // MEMCONTROL2
    // CACHECONTROL
    // DMA
    // GPU
    // TIMERS
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

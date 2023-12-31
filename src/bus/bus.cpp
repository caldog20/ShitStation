#include "bus.hpp"

#include "cdrom/cdrom.hpp"
#include "cpu/cpu.hpp"
#include "dma/dmacontroller.hpp"
#include "gpu/gpu.hpp"
#include "sio/sio.hpp"
#include "spu/spu.hpp"
#include "support/log.hpp"
#include "timers/timers.hpp"

namespace Bus {

Bus::Bus(Cpu::Cpu& cpu, DMA::DMA& dma, Timers::Timers& timers, CDROM::CDROM& cdrom, SIO::SIO& sio, GPU::GPU& gpu, Spu::Spu& spu)
    : cpu(cpu), dma(dma), timers(timers), gpu(gpu), cdrom(cdrom), sio(sio), spu(spu) {
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

    if (PAD.contains(hw_address)) {
        auto offset = PAD.offset(hw_address);
        return sio.read<u8>(offset);
    }

    if (EXP2.contains(hw_address)) {
        return 0xff;
    }

    if (EXP1.contains(hw_address)) {
        return 0xff;
    }

    if (CDROM.contains(hw_address)) {
        auto offset = CDROM.offset(hw_address);
        return cdrom.read(offset);
    }

    Log::warn("[BUS] [READ8] Unhandled read at address: {:08x}\n", hw_address);
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

    // Temp timer2 stub
    if (hw_address == 0x1f801120) {
        return 0x000016b0;
    }

    if (IRQCONTROL.contains(hw_address)) {
        auto offset = IRQCONTROL.offset(hw_address);
        if (offset == 0) {
            return ISTAT;
        } else if (offset == 4) {
            return IMASK;
        }
    }

    if (CDROM.contains(hw_address)) {
        auto offset = CDROM.offset(hw_address);
        u8 value1 = cdrom.read(offset);
        u8 value2 = cdrom.read(offset);
        return (value1 << 8) | value2;
    }

    if (SPU.contains(hw_address)) {
        return spu.read16(hw_address);
    }

    if (TIMERS.contains(hw_address)) {
        auto offset = TIMERS.offset(hw_address);
        //        return timers.read(offset);
        return 0;
    }

    if (PAD.contains(hw_address)) {
        auto offset = PAD.offset(hw_address);
        return sio.read<u16>(offset);
    }

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
            return gpu.read0();
        } else if (offset == 4) {
            return gpu.read1();
        }
        return 0;
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
        return 0;
    }

    if (PAD.contains(hw_address)) {
        auto offset = PAD.offset(hw_address);
        return sio.read<u32>(offset);
    }

    if (MEMCONTROL.contains(hw_address)) {
        auto offset = MEMCONTROL.offset(hw_address);
        return MemControl[offset >> 2];
    }

    if (SPU.contains(hw_address)) {
        return spu.read32(hw_address);
    }

    if (EXP1.contains(hw_address)) {
        return 0xff;
    }

    Log::warn("[BUS] [READ32] Unhandled read at address: {:08x}\n", address);
    return 0;
}

void Bus::write8(u32 address, u8 value) {
    const auto page = address >> 16;
    const auto offset = address & 0xFFFF;
    const auto pointer = writePages[page];

    if (cpu.isCacheIsolated()) return;

    cpu.addCycles(CycleBias::RAM);

    // RAM Fastmem Writes
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

    if (CDROM.contains(hw_address)) {
        auto offset = CDROM.offset(hw_address);
        cdrom.write(offset, value);
        return;
    }

    if (PAD.contains(hw_address)) {
        auto offset = PAD.offset(hw_address);
        return sio.write<u8>(offset, value);
    }

    if (SPU.contains(hw_address)) {
        return spu.write8(hw_address, value);
    }

    if (EXP2.contains(hw_address)) {
        return;
    }

    Log::warn("[BUS] [WRITE8] Unhandled write at address: {:08x} : value: {:08x}\n", address, value);
}

void Bus::write16(u32 address, u16 value) {
    const auto page = address >> 16;
    const auto offset = address & 0xFFFF;
    const auto pointer = writePages[page];

    if (cpu.isCacheIsolated()) return;

    cpu.addCycles(CycleBias::RAM);

    // RAM Fastmem Writes
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
        return spu.write16(hw_address, value);
    }

    if (TIMERS.contains(hw_address)) {
        auto offset = TIMERS.offset(hw_address);
        timers.write(offset, value);
        return;
    }

    if (PAD.contains(hw_address)) {
        auto offset = PAD.offset(hw_address);
        return sio.write<u16>(offset, value);
    }
    // SPU
    Log::warn("[BUS] [WRITE16] Unhandled write at address: {:08x} : value: {:08x}\n", address, value);
}

void Bus::write32(u32 address, u32 value) {
    const auto page = address >> 16;
    const auto offset = address & 0xFFFF;
    const auto pointer = writePages[page];

    if (cpu.isCacheIsolated()) return;

    cpu.addCycles(CycleBias::RAM);

    // RAM Fastmem Writes
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

    if (PAD.contains(hw_address)) {
        auto offset = PAD.offset(hw_address);
        return sio.write<u32>(offset, value);
    }

    if (GPU.contains(hw_address)) {
        auto offset = GPU.offset(hw_address);
        if (offset == 0) {
            gpu.write0(value);
        } else if (offset == 4) {
            gpu.write1(value);
        }
        return;
    }

    if (SPU.contains(hw_address)) {
        spu.write16(hw_address, value & 0xFFFF);
        spu.write16(hw_address, value >> 16);
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

    std::memcpy(ram + addr, sideloadEXE.data(), size);
}

}  // namespace Bus

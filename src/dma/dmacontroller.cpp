#include "dmacontroller.hpp"

#include <cassert>
#include <memory>

#include "bus/bus.hpp"
#include "cdrom/cdrom.hpp"
#include "gpu/gpu.hpp"
#include "spu/spu.hpp"
#include "scheduler/scheduler.hpp"
#include "support/log.hpp"

namespace DMA {

DMA::DMA(Bus::Bus& bus, Scheduler::Scheduler& scheduler) : bus(bus), scheduler(scheduler) { reset(); }

void DMA::reset() {
    dpcr = 0x07654321;
    std::memset(&dicr, 0, sizeof(dicr));
    std::memset(channels, 0, sizeof(channels));
}

u32 DMA::read(u32 offset) {
    switch (offset) {
        case 0x70: return dpcr;
        case 0x74: {
            u32 value = 0;
            value = static_cast<u32>(dicr.forceIRQ << 15);
            value |= static_cast<u32>(dicr.im << 16);
            value |= static_cast<u32>(dicr.masterIRQEnable << 23);
            value |= static_cast<u32>(dicr.ip << 24);
            value |= static_cast<u32>(dicr.masterIRQFlag << 31);
            return value;
        }
    }

    auto channel = (offset & 0x70) >> 4;
    auto& ch = channels[channel];

    switch (offset & 0xF) {
        case 0: return ch.base;
        case 4: return static_cast<u32>(ch.blockSize | (ch.blockCount << 16));
        case 8:
            u32 value = 0;
            value = static_cast<u32>(ch.direction);
            value |= ch.step << 1;
            if (ch.chop) {
                value |= 1 << 8;
            }
            value |= static_cast<u32>(ch.sync << 9);
            value |= static_cast<u32>(ch.chopDMASize << 16);
            value |= static_cast<u32>(ch.chopCPUSize << 20);
            if (ch.start) {
                value |= 1 << 24;
            }
            if (ch.trigger) {
                value |= 1 << 28;
            }
            return value;
    }

    Helpers::panic("[DMA] Unhandled DMA read32 at offset {:#x}\n", offset);
    return 0;
}

void DMA::write8(u32 offset, u8 value) {
    switch (offset & 3) {
        case 1: dicr.forceIRQ = Helpers::isBitSet(value, 7); break;
        case 2:
            dicr.im = value & 0x7F;
            dicr.masterIRQEnable = Helpers::isBitSet(value, 7);
            break;
        case 3: dicr.ip = (dicr.ip & ~value) & 0x7F; break;
    }
    //    checkIRQ();
}

void DMA::write(u32 offset, u32 value) {
    switch (offset) {
        case 0x70: dpcr = value; return;
        case 0x74: {
            dicr.forceIRQ = Helpers::isBitSet(value, 15);
            dicr.masterIRQEnable = Helpers::isBitSet(value, 23);
            dicr.im = (value >> 16) & 0x7F;
            dicr.ip = (dicr.ip & ~(value >> 24)) & 0x7F;
            checkIRQ();
            return;
        }
    }

    auto channel = (offset & 0x70) >> 4;
    auto& ch = channels[channel];

    switch (offset & 0xF) {
        case 0: ch.base = value & 0xffffff; return;
        case 4:
            ch.blockSize = static_cast<u16>(value);
            ch.blockCount = static_cast<u16>(value >> 16);
            return;
        case 8:
            ch.direction = Helpers::isBitSet(value, 0) ? Direction::FromRam : Direction::ToRam;
            ch.step = Helpers::isBitSet(value, 1) ? Step::Decrement : Step::Increment;
            ch.chop = (value >> 8) & 1;
            ch.sync = static_cast<SyncMode>((value >> 9) & 3);
            ch.chopDMASize = static_cast<u8>(value >> 16) & 7;
            ch.chopCPUSize = static_cast<u8>(value >> 20) & 7;
            ch.start = Helpers::isBitSet(value, 24);
            ch.trigger = Helpers::isBitSet(value, 28);
            checkChannelActive(static_cast<Port>(channel));
            return;
    }

    Helpers::panic("[DMA] Unhandled DMA write32 at offset: {:#x} value: {:#08x}\n", offset, value);
}

void DMA::checkIRQ() {
    const auto prevMIF = dicr.masterIRQFlag;

    dicr.masterIRQFlag = dicr.forceIRQ || (dicr.masterIRQEnable && (dicr.im & dicr.ip));

    if (!prevMIF && dicr.masterIRQFlag) {
        scheduler.scheduleInterrupt(1000, Bus::IRQ::DMA);
    }
}

void DMA::checkChannelActive(Port port) {
    auto& ch = channels[static_cast<u32>(port)];
    bool trigger = ch.sync == SyncMode::Manual ? ch.trigger : true;
    if (ch.start && trigger) {
        startDMA(ch, port);
    }
}

void DMA::startDMA(Channel& channel, Port port) {
    if (channel.sync == SyncMode::LinkedList) {
        dmaLinkedList(channel, port);
    } else {
        dmaBlockCopy(channel, port);
    }
}

void DMA::dmaLinkedList(Channel& channel, Port port) {
    //    Log::debug("[DMA] DMA linked list\n");
    u32 address = channel.base;
    assert(port == Port::GPU);
    u32 header = 0;
    while (true) {
        header = bus.read<u32>(address);
        u32 size = header >> 24;
        while (size > 0) {
            address = (address + 4) & 0x1FFFFC;
            u32 value = bus.read<u32>(address);
            bus.gpu.write0(value);
            size--;
        }

        if ((header & 0x800000) != 0) {
            break;
        }
        address = header & 0x1FFFFC;
    }
    transferFinished(channel, port);
}

void DMA::dmaBlockCopy(Channel& channel, Port port) {
    //    Log::debug("[DMA] DMA block copy\n");
    int step = channel.step == Step::Increment ? 4 : -4;
    u32 address = channel.base & 0xFFFFFF;
    u32 remsize = getTransferSize(channel);
    while (remsize > 0) {
        u32 addr = address & 0x1FFFFC;
        u32 value = 0;
        if (channel.direction == Direction::ToRam) {
            switch (port) {
                case Port::OTC: {
                    if (remsize == 1) {
                        value = 0xffffff;
                    } else {
                        value = (addr - 4) & 0x1FFFFC;
                    }
                    bus.write<u32>(addr, value);
                    break;
                }
                case Port::GPU: {
                    value = bus.gpu.read0();
                    bus.write<u32>(addr, value);
                    break;
                }
                case Port::CDROM: {
                    value = bus.cdrom.dmaRead();
                    bus.write<u32>(addr, value);
                    break;
                }
                case Port::SPU: {
                    value = bus.spu.readRAM();
                    value |= bus.spu.readRAM() << 16;
                    bus.write<u32>(addr, value);
                    break;
                }
            }
        } else if (channel.direction == Direction::FromRam) {
            value = bus.read<u32>(addr);
            switch (port) {
                case Port::GPU: {
                    bus.gpu.write0(value);
                    break;
                }
                case Port::SPU: {
                    bus.spu.pushFifo(static_cast<u16>(value));
                    bus.spu.pushFifo(static_cast<u16>(value >> 16));
                    break;
                }
            }
        }

        address += step;
        remsize--;
    }
    transferFinished(channel, port);
}

void DMA::transferFinished(Channel& channel, Port port) {
    channel.start = false;
    channel.trigger = false;

    if (dicr.im & (1 << static_cast<u8>(port))) {
        dicr.ip |= (1 << static_cast<u8>(port));
        checkIRQ();
    }
}

u32 DMA::getTransferSize(Channel& channel) {
    if (channel.sync == SyncMode::Manual) {
        return static_cast<u32>(channel.blockSize);
    } else if (channel.sync == SyncMode::Request) {
        return channel.blockSize * channel.blockCount;
    }
}

}  // namespace DMA

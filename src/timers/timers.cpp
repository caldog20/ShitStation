#include "timers.hpp"

#include "scheduler/scheduler.hpp"

namespace Timers {

Timers::Timers(Scheduler::Scheduler& scheduler) : scheduler(scheduler) { reset(); }

void Timers::reset() { std::memset(timers, 0, sizeof(timers)); }

u16 Timers::read(u32 offset) {
    auto& timer = timers[offset >> 4];

    switch (offset & 0xF) {
        case 0: return timer.counter;
        case 4: {
            u16 value = 0;
            value = timer.syncEnable;
            value |= timer.syncMode << 1;
            value |= timer.targetWrap << 3;
            value |= timer.irqTarget << 4;
            value |= timer.irqMax << 5;
            value |= timer.irqRepeat << 6;
            value |= timer.irqPulse << 7;
            value |= timer.clockSource << 8;
            value |= timer.irq << 10;
            value |= timer.atTarget << 11;
            value |= timer.atMax << 12;

            timer.atTarget = false;
            timer.atMax = false;
            return value;
        }
        case 8: return timer.target;
    }

    return 0;
}

void Timers::write(u32 offset, u16 value) {
    auto& timer = timers[offset >> 4];

    switch (offset & 0xF) {
        case 0:
            timer.counter = value;
            timer.atMax = false;
            timer.atTarget = false;
            timer.irq = false;
            break;
        case 4: {
            timer.syncEnable = Helpers::isBitSet(value, 1);
            timer.syncMode = (value >> 1) & 3;
            timer.targetWrap = Helpers::isBitSet(value, 3);
            timer.irqTarget = Helpers::isBitSet(value, 4);
            timer.irqMax = Helpers::isBitSet(value, 5);
            timer.irqRepeat = Helpers::isBitSet(value, 6);
            timer.irqPulse = Helpers::isBitSet(value, 7);
            timer.clockSource = (value >> 8) & 3;
            timer.irq = true;  // Set to 1 after writing mode
            timer.counter = 0;
            break;
        }
        case 8:
            timer.target = value;
            timer.atMax = false;
            timer.atTarget = false;
            timer.irq = false;
            timer.counter = 0;  // Reset timer value
            break;
    }
}

void Timers::update() { auto& timer = timers[2]; }

}  // namespace Timers

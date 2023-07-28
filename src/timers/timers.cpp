#include "timers.hpp"

#include "scheduler/scheduler.hpp"

namespace Timers {

Timers::Timers(Scheduler::Scheduler& scheduler) : scheduler(scheduler) { reset(); }

void Timers::reset() {
    //    std::memset(timers, 0, sizeof(timers));
}

u16 Timers::read(u32 address) { return 0; }

void Timers::write(u32 address, u16 value) {}

void Timers::update() {}

}  // namespace Timers

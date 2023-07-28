#pragma once
#include "support/helpers.hpp"

namespace Scheduler {
class Scheduler;
}

namespace Timers {

struct Timer {
    u16 counter;
    u16 target;

    bool syncEnable;
    u8 syncMode;
    bool targetWrap;
    bool irqTarget;
    bool irqMax;
    bool irqRepeat;
    bool irqPulse;
    u8 clockSource;
    bool irq;
    bool atTarget;
    bool atMax;
};

class Timers {
  public:
    Timers(Scheduler::Scheduler& scheduler);

    void reset();

    u16 read(u32 offset);
    void write(u32 offset, u16 value);

    void update();

  private:
    Timer timers[3];
    Scheduler::Scheduler& scheduler;
};

}  // namespace Timers

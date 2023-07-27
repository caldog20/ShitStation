#pragma once
#include <functional>
#include <queue>

#include "bus/bus.hpp"
#include "cpu/cpu.hpp"

namespace Scheduler {
using Cycles = std::uint64_t;
using Callback = std::function<void()>;

class Event {
  public:
    Event(Cycles cycles) : targetCycles(cycles) {}
    Event(Cycles cycles, Callback callback) : targetCycles(cycles), callback(callback) {}
    Event() : Event(std::numeric_limits<Cycles>::max()) {}

    bool operator>(const Event& rhs) const { return targetCycles > rhs.targetCycles; }
    bool operator<(const Event& rhs) const { return targetCycles < rhs.targetCycles; }

    [[nodiscard]] Cycles cycleTarget() const { return targetCycles; }
    void setCycleTarget(Cycles cycles) { targetCycles = cycles; }

    Cycles targetCycles;
    Callback callback = nullptr;
};

class Scheduler {
  public:
    explicit Scheduler(Bus::Bus& bus, Cpu::Cpu& cpu);

    void reset();

    void clearEvents() {
        while (!events.empty()) {
            events.pop();
        }
    }

    void scheduleEvent(Cycles cycleCount, Callback callback);

    void scheduleInterrupt(Cycles cycleCount, Bus::IRQ irq);

    void handleEvents();

    auto nextEventCycles() { return events.top().cycleTarget(); }

  private:
    Bus::Bus& bus;
    Cpu::Cpu& cpu;
    Cycles& cycles;
    std::priority_queue<Event, std::vector<Event>, std::greater<>> events;
};

}  // namespace Scheduler

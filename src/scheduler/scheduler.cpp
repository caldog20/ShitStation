#include "scheduler.hpp"

namespace Scheduler {
	Scheduler::Scheduler(Bus::Bus& bus, Cpu::Cpu& cpu) : bus(bus), cpu(cpu), cycles(cpu.getCycleRef()) { reset(); }

	void Scheduler::scheduleEvent(Cycles cycleCount, Callback callback) {
		events.emplace(cycles + cycleCount, callback);
		cpu.setCycleTarget(events.top().cycleTarget());
	}

	void Scheduler::handleEvents() {
		while (cycles >= events.top().cycleTarget()) {
			if (events.top().callback != nullptr) events.top().callback();
			events.pop();
		}
	}

	void Scheduler::reset() {
		clearEvents();
		scheduleEvent(std::numeric_limits<Cycles>::max(), nullptr);
	}

	void Scheduler::scheduleInterrupt(Cycles cycleCount, Bus::IRQ irq) {
		scheduleEvent(cycleCount, [&]() { bus.triggerInterrupt(irq); });
	}

}  // namespace Scheduler

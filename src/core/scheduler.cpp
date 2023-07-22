#include "scheduler.hpp"

Scheduler::Scheduler(Cycles& cycles) : cycles(cycles) { scheduleEvent(std::numeric_limits<Cycles>::max(), nullptr); }

void Scheduler::scheduleEvent(Cycles cycleCount, Callback callback) { events.emplace(cycles + cycleCount, callback); }

void Scheduler::handleEvents() {
		while (cycles >= events.top().cycleTarget()) {
				if (events.top().callback != nullptr) events.top().callback();

				events.pop();
		}
}

void Scheduler::reset() { clearEvents(); }

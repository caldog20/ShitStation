#pragma once
#include <functional>
#include <queue>

#include "core/events.hpp"

class Scheduler {
	public:
		explicit Scheduler(Cycles& cycles);

		void reset();

		void clearEvents() {
				while (!events.empty()) {
						events.pop();
				}
		}

		void scheduleEvent(Cycles cycleCount, Callback callback);

		void handleEvents();

		auto nextEventCycles() { return events.top().cycleTarget(); }

	private:
		Cycles& cycles;
		std::priority_queue<Event, std::vector<Event>, std::greater<>> events;
};

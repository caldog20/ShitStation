#pragma once
#include <functional>
#include <limits>
#include <cstdint>

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

#include "core/emulator.hpp"

auto main() -> int {
	Emulator emulator;
	while (emulator.isOpen()) {
		emulator.update();
	}
}

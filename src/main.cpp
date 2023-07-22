#include "core/emulator.hpp"

auto main() -> int {
		Emulator emulator;
		emulator.bus.loadBIOS("/Users/yatesca/Projects/PSXEmulator/resources/SCPH1001.BIN");
		while (emulator.isOpen()) {
				emulator.update();
		}
}

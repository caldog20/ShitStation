#include <filesystem>

#include "psx.hpp"

auto main() -> int {
    PSX psx;
    psx.loadBIOS(std::filesystem::current_path() / "SCPH1001.BIN");
    psx.sideload("../psxtest_cpu.exe");
    psx.start();

    while (psx.isOpen()) {
        psx.update();
    }

    return 0;
}

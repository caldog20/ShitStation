#include "playstation.hpp"

auto main() -> int {
    Playstation psx;
    psx.loadBIOS("../SCPH1001.BIN");
    psx.sideload("../psxtest_cpu.exe");
    psx.start();

    while (true) {
        psx.update();
    }

    return 0;
}

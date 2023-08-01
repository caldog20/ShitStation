#include "psx.hpp"

auto main() -> int {
    PSX psx;
    psx.loadBIOS("../SCPH1001.BIN");
    //    psx.sideload("../padtest.exe");
    psx.start();

    while (psx.isOpen()) {
        psx.update();
    }

    return 0;
}

#include "psx.hpp"

auto main() -> int {
    PSX psx;
    psx.loadBIOS("../SCPH1001.BIN");
    //    psx.sideload("../pong.ps-exe");
    psx.start();

    while (true) {
        psx.update();
    }

    return 0;
}

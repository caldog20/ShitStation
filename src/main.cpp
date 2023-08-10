#include "psx.hpp"

auto main() -> int {
    PSX psx;
    psx.loadBIOS("../SCPH1001.BIN");
    //        psx.sideload("../tetris.ps-exe");
    //    psx.loadDisc("/Users/yatesca/Projects/PSXEmulator/bam2.bin");
    //        psx.loadDisc("/Users/yatesca/Emulation/yopazicestar/yicestar_ntsc.bin");
    psx.start();

    while (psx.isOpen()) {
        psx.update();
    }

    return 0;
}

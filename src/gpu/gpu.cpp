#include "gpu.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <queue>
#include <vector>

namespace GPU {




 GPU::GPU() { reset(); }

 GPU::~GPU() {}

 void GPU::reset() {
    drawMode = 0;
    gpustat = 0x14802000;
    gpuread = 0;
    command = 0;
    commandPending = false;
    argsNeeded = 0;
    argsReceived = 0;
    rectTexpage = 0;
    transferRect = {0, 0, 0, 0};
    transferSize = 0;
    transferIndex = 0;
    readMode = Command;
    writeMode = Command;
    rectTexpage = 0;
    vram.resize(VRAM_SIZE);
    transferReadBuffer.resize(VRAM_SIZE);
    transferWriteBuffer.resize(VRAM_SIZE);
}

 u32 GPU::read1() { return 0b01011110100000000000000000000000; }

 u32 GPU::read0() {
    if (readMode == Transfer) {
        auto value = transferReadBuffer[transferIndex++];
        if (transferSize-- == 0) {
            readMode = Command;
        }
        return value;
    } else {
        return gpuread;
    }
}

 void GPU::write0(u32 value) {
    if (writeMode == Command) {
        if (commandPending) {
            args.push_back(value);
            argsReceived++;
        } else {
            command = value >> 24;
            argsNeeded = params[command];
            if (argsNeeded == 0) {
                writeInternal(value);
                return;
            }
            commandPending = true;
            args.clear();
            args.push_back(value);
            argsReceived = 0;
        }

        if (argsReceived != argsNeeded) return;

        // process command here
        drawCommand();
    } else if (writeMode == Transfer) {
        transferWriteBuffer.push_back(value);
        if (--transferSize == 0) {
            // transfer to vram and reset write mode
            writeMode = Command;
        }
    }
}

 void GPU::write1(u32 value) {}

 void GPU::drawCommand() {}

 void GPU::writeInternal(u32 value) {


}

}  // namespace GPU

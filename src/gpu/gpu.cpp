#include "gpu.hpp"

#include <algorithm>

namespace GPU {

GPU::GPU() {}

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
    dmaDirection = Off;
    dmaRequest = 0;
    hres = 0;
    vres = static_cast<VRes>(0);
    videoMode = static_cast<VideoMode>(0);
    drawMode = 0;
    drawArea = {0, 0, 0, 0};
    drawOffset.x() = 0;
    drawOffset.y() = 0;
    transferRect = {0, 0, 0, 0};
    transferSize = 0;
    transferIndex = 0;
    readMode = Command;
    writeMode = Command;
    vram.resize(VRAM_SIZE);
    transferReadBuffer.resize(VRAM_SIZE);
    transferWriteBuffer.resize(VRAM_SIZE);

    dither = false;
    drawToDisplay = false;
    setMaskBit = false;
    preserveMaskedPixels = false;
    interlaced = false;
    disableDisplay = false;
    irq = false;
    interlaceField = false;
    inVblank = false;
    inHblank = false;
    textureDisable = false;
    rectTextureFlipX = false;
    rectTextureFlipY = false;

    drawMode = 0;
    texPageX = 0;
    texPageY = 0;
    semiTrans = 0;
    textureDepth = static_cast<TextureDepth>(0);
    displayDepth = static_cast<DisplayDepth>(0);
}

u32 GPU::read1() { return gpustat; }

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

        drawCommand();
        commandPending = false;
    } else if (writeMode == Transfer) {
        transferWriteBuffer.push_back(value);
        if (--transferSize == 0) {
            transferToVram();
            writeMode = Command;
        }
    }
}

void GPU::write1(u32 value) {
    auto index = (value >> 24) & 0xFF;
    switch (index) {
        // Reset GPU
        case 0x0:
            //            reset();
            break;
        // Reset Command Fifo
        case 0x1: resetFifo(); break;
        // Ack IRQ
        case 0x2: ackIRQ(); break;
        // Display Enable
        case 0x3: setDisplayEnable(value); break;
        // DMA Direction
        case 0x4: setDmaDirection(value); break;
        // Start of Display Area
        case 0x5: setDisplayAreaStart(value); break;
        // Horizontal Display Range
        case 0x6: setDisplayHorizontalRange(value); break;
        // Vertical Display Range
        case 0x7: setDisplayVerticalRange(value); break;
        // Display Mode
        case 0x8:
            setDisplayMode(value);
            break;
            // GPU Info
        case 0x10: setGPUInfo(value); break;
        default: Log::warn("[GPU] GP1 Internal - Unhandled command: {:#02X}", index);
    }
    updateGPUStat();
}

void GPU::writeInternal(u32 value) {
    switch (command) {
        // NOP
        case 0x00: break;
        // Clear texture Cache
        case 0x01: break;
        // Draw Mode
        case 0xE1: setDrawMode(value); break;
        // Texture Window
        case 0xE2: setTextureWindow(value); break;
        // Draw Area Top/Left
        case 0xE3: setDrawAreaTopLeft(value); break;
        // Draw Area Bottom/Right
        case 0xE4: setDrawAreaBottomRight(value); break;
        // Draw Offset
        case 0xE5: setDrawOffset(value); break;
        // Mask Bit Setting
        case 0xE6: setMaskBitSetting(value); break;
        default: Log::warn("[GPU] GP0 Internal - Unhandled command: {:#02X}", command);
    }
    updateGPUStat();
}

void GPU::updateGPUStat() {
    gpustat |= drawMode & 0x7FF;
    gpustat |= static_cast<u32>(Helpers::isBitSet(drawMode, 11) << 15);
    gpustat |= static_cast<u32>(setMaskBit << 11);
    gpustat |= static_cast<u32>(preserveMaskedPixels << 12);
    gpustat |= static_cast<u32>(interlaceField << 13);
    gpustat |= hres << 16;
    gpustat |= vres << 19;
    gpustat |= videoMode << 20;
    gpustat |= displayDepth << 21;
    gpustat |= static_cast<u32>(interlaced << 22);
    gpustat |= static_cast<u32>(disableDisplay << 23);
    gpustat |= static_cast<u32>(irq << 24);

    gpustat |= 1 << 26;
    gpustat |= 1 << 27;
    gpustat |= 1 << 28;

    gpustat |= dmaDirection << 29;

    switch (dmaDirection) {
        case Off: dmaRequest = 0; break;
        case Fifo: dmaRequest = 1; break;
        case CputoGpu: dmaRequest = (gpustat >> 28) & 1; break;
        case GputoCpu: dmaRequest = (gpustat >> 27) & 1; break;
    }

    gpustat |= 1 << 30;
    // TODO: Fix this later
    gpustat |= static_cast<u32>(1 << 31);

    gpustat |= dmaRequest << 25;
}

void GPU::blankDraw() {}

void GPU::setTextureWindow(u32 value) {
    // 8 pixel steps - multiply by 8
    texWindow.xMask = value & 0x1F;
    texWindow.yMask = (value >> 5) & 0x1F;
    texWindow.x = (value >> 10) & 0x1F;
    texWindow.y = (value >> 15) & 0x1F;
}

void GPU::setDrawOffset(u32 value) {
    u16 x = value & 0x7FF;
    u16 y = (value >> 11) & 0x7FF;

    drawOffset.x() = static_cast<s16>(x << 5) >> 5;
    drawOffset.y() = static_cast<s16>(y << 5) >> 5;
}

void GPU::setDrawAreaTopLeft(u32 value) {
    drawArea.top = (value >> 10) & 0x3FF;
    drawArea.left = value & 0x3FF;
}

void GPU::setDrawAreaBottomRight(u32 value) {
    drawArea.bottom = (value >> 10) & 0x3FF;
    drawArea.right = value & 0x3FF;
}

void GPU::setDrawMode(u32 value) {
    drawMode = static_cast<u16>(value);
    texPageX = value & 0xF;
    texPageY = (value >> 4) & 0x1;
    semiTrans = (value >> 5) & 3;

    textureDepth = static_cast<TextureDepth>((value >> 7) & 3);
    dither = Helpers::isBitSet(value, 9);
    drawToDisplay = Helpers::isBitSet(value, 10);
    textureDisable = Helpers::isBitSet(value, 11);
    rectTextureFlipX = Helpers::isBitSet(value, 12);
    rectTextureFlipY = Helpers::isBitSet(value, 13);
    rectTexpage = value & 0x3fff;
}

void GPU::setMaskBitSetting(u32 value) {
    setMaskBit = Helpers::isBitSet(value, 0);
    preserveMaskedPixels = Helpers::isBitSet(value, 1);
}

void GPU::resetFifo() {
    args.clear();
    argsReceived = 0;
    argsNeeded = 0;
    commandPending = false;
    writeMode = Command;
}

void GPU::ackIRQ() { irq = false; }

void GPU::setDisplayEnable(u32 value) { disableDisplay = Helpers::isBitSet(value, 0); }

void GPU::setDmaDirection(u32 value) { dmaDirection = static_cast<DmaDirection>(value & 3); }

void GPU::setDisplayAreaStart(u32 value) {
    displayStart.x() = value & 0x3FF;
    displayStart.y() = (value >> 10) & 0x1FF;
}

void GPU::setDisplayHorizontalRange(u32 value) {
    displayHRange.x() = value & 0xFFF;
    displayHRange.y() = (value >> 12) & 0xFFF;
}

void GPU::setDisplayVerticalRange(u32 value) {
    displayVRange.x() = value & 0x3FF;
    displayVRange.y() = (value >> 10) & 0x3FF;
}

void GPU::setDisplayMode(u32 value) {
    u8 hres1 = value & 3;
    u8 hres2 = (value >> 6) & 1;
    hres = static_cast<u32>((hres2 & 1) | ((hres1 & 3) << 1));

    vres = static_cast<VRes>(Helpers::isBitSet(value, 2));
    videoMode = static_cast<VideoMode>(Helpers::isBitSet(value, 3));
    displayDepth = static_cast<DisplayDepth>(value & 0x10);
    interlaced = value & 0x20;
    interlaceField = true;
}

void GPU::setGPUInfo(u32 value) { gpuread = 1; }

}  // namespace GPU

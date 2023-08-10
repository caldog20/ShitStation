#pragma once
#include <array>
#include <cassert>
#include <vector>

#include "support/helpers.hpp"
#include "support/log.hpp"
#include "support/opengl.hpp"

namespace Scheduler {
class Scheduler;
}

namespace GPU {

template <class T>
concept Scalar = std::is_scalar<T>::value;
template <Scalar T>
struct Rect {
    T x;
    T y;
    T w;
    T h;

    Rect() : x(0), y(0), w(0), h(0) {}
    Rect(T x, T y, T w, T h) : x(x), y(y), w(w), h(h) {}
};

struct DrawArea {
    u16 left;
    u16 top;
    u16 right;
    u16 bottom;
};

struct TextureWindow {
    u16 x;
    u16 y;
    u16 xMask;
    u16 yMask;
};

struct Point {
    u32 pos;
    u32 color;

    Point() : pos(0), color(0) {}
    Point(u32 pos, u32 color) : pos(pos), color(color) {}
};

class GPU {
  public:
    GPU(Scheduler::Scheduler& scheduler);
    virtual ~GPU();

    virtual void reset();

    u32 read0();
    u32 read1();

    void write0(u32 value);
    void write1(u32 value);

    static constexpr int VRAM_WIDTH = 1024;
    static constexpr int VRAM_HEIGHT = 512;
    static constexpr int VRAM_SIZE = VRAM_WIDTH * VRAM_HEIGHT;

  protected:
    enum class Shading { None, Flat, Gouraud, TexBlendFlat, TexBlendGouraud, RawTex, RawTexGouraud };
    enum class Transparency { Opaque, Transparent };
    enum class Polygon { Triangle, Quad };
    enum class Rectsize { Rect1, Rect8, Rect16, RectVariable };

    enum TextureDepth : u32 { T4, T8, T16 };
    enum DisplayDepth : u32 { D15, D24 };
    enum DmaDirection : u32 { Off, Fifo, CputoGpu, GputoCpu };
    enum GP0Mode { Command, Transfer };
    enum HRes : u32 { H256 = 0, H320 = 1, H512 = 2, H640 = 3 };
    enum VRes : u32 { V240 = 0, V480 = 1 };
    enum VideoMode : u32 { NTSC = 0, PAL = 1 };

    void updateGPUStat();

    virtual void drawCommand() = 0;
    virtual void transferToVram() = 0;
    virtual void transferToCpu() = 0;
    virtual void TransferVramToVram() = 0;
    virtual void internalCommand(u32 value) = 0;

    virtual void setDrawMode(u32 value) = 0;
    virtual void setTextureWindow(u32 value) = 0;
    virtual void setDrawOffset(u32 value) = 0;
    virtual void setDrawAreaTopLeft(u32 value) = 0;
    virtual void setDrawAreaBottomRight(u32 value) = 0;
    virtual void setMaskBitSetting(u32 value) = 0;

    // GP1 Internal Commands
    void resetFifo();
    void ackIRQ();
    void setDisplayEnable(u32 value);
    void setDmaDirection(u32 value);
    void setDisplayAreaStart(u32 value);
    void setDisplayHorizontalRange(u32 value);
    void setDisplayVerticalRange(u32 value);
    void setDisplayMode(u32 value);
    void setGPUInfo(u32 value);

    u16 drawMode;
    u8 texPageX;
    u8 texPageY;
    u8 semiTrans;

    TextureDepth textureDepth;
    DisplayDepth displayDepth;

    bool dither;
    bool drawToDisplay;
    bool setMaskBit;
    bool preserveMaskedPixels;
    bool interlaced;
    bool disableDisplay;
    bool irq;
    bool interlaceField;
    bool inVblank;
    bool inHblank;
    bool textureDisable;
    bool rectTextureFlipX;
    bool rectTextureFlipY;

    u32 dmaRequest;

    u64 cycles = 0;
    u64 lines = 0;

    DmaDirection dmaDirection;
    OpenGL::Vector<u16, 2> displayStart;
    OpenGL::Vector<u16, 2> displayHRange;
    OpenGL::Vector<u16, 2> displayVRange;
    Rect<u16> transferRect;
    DrawArea drawArea;
    OpenGL::Vector<s16, 2> drawOffset;
    TextureWindow texWindow;

    u32 hres;
    VRes vres;
    VideoMode videoMode;

    u32 gpustat;
    u32 gpuread;

    u8 command;
    u8 argsNeeded;
    u8 argsReceived;

    std::vector<u32> args;
    std::vector<u32> transferWriteBuffer;
    std::vector<u32> transferReadBuffer;
    u32 transferSize;
    u32 transferIndex;

    std::vector<u16> vram;

    u16 rectTexpage;
    bool commandPending = false;

    GP0Mode readMode = Command;
    GP0Mode writeMode = Command;

    // GPU Command Param LUT
    static constexpr int params[256] = {
        0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x03, 0x06, 0x06, 0x06, 0x06, 0x04, 0x04, 0x04, 0x04,
        0x08, 0x08, 0x08, 0x08, 0x05, 0x05, 0x05, 0x05, 0x08, 0x08, 0x08, 0x08, 0x07, 0x07, 0x07, 0x07, 0x0B, 0x0B, 0x0B, 0x0B, 0x02, 0x02,
        0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x04, 0x04, 0x04, 0x04, 0x05, 0x05, 0x05, 0x05, 0x03, 0x03, 0x03, 0x03, 0x00, 0x00, 0x00, 0x00,
        0x06, 0x06, 0x06, 0x06, 0x08, 0x08, 0x08, 0x08, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03, 0x01, 0x01, 0x01, 0x01, 0x00, 0x00,
        0x00, 0x00, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x01, 0x01, 0x01, 0x01, 0x02, 0x02, 0x02, 0x02, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x03,
        0x03, 0x03, 0x03, 0x03, 0x03, 0x03, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02,
        0x02, 0x02, 0x02, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };

    Scheduler::Scheduler& scheduler;
};

}  // namespace GPU

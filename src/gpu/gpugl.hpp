#pragma once
#include <vector>

#include "gpu.hpp"

namespace GPU {

struct Vertex {
    OpenGL::Vector<GLint, 2> position;
    u32 color;
    u16 texpage;
    u16 clut;
    OpenGL::Vector<GLushort, 2>  texcoords;

    Vertex() : position({0, 0}), color(0), texpage(0), clut(0), texcoords({0, 0}) {}
    Vertex(u32 pos, u32 color) : color(color) {
        setPosition(pos);
        texpage = 0x8000;
    }

    Vertex(u32 x, u32 y, u32 color) : color(color) {
        position.x() = Helpers::signExtend16(x, 11);
        position.y() = Helpers::signExtend16(y, 11);
        texpage = 0x8000;
    }

    Vertex(u32 x, u32 y, u32 color, u16 texpage, u16 clut, u16 texCoords) : color(color), texpage(texpage), clut(clut) {
        position.x() = Helpers::signExtend16(x, 11);
        position.y() = Helpers::signExtend16(y, 11);
        texcoords.x() = texCoords & 0xFF;
        texcoords.y() = (texCoords >> 8) & 0xFF;
    }

    Vertex(u32 x, u32 y, u32 color, u16 texpage, u16 clut, u16 tx, u16 ty) : color(color), texpage(texpage), clut(clut) {
        position.x() = Helpers::signExtend16(x, 11);
        position.y() = Helpers::signExtend16(y, 11);
        texcoords.x() = tx;
        texcoords.y() = ty;
    }

    Vertex(u32 pos, u32 color, u16 texpage, u16 clut, u16 texCoords) : color(color), texpage(texpage), clut(clut) {
        setPosition(pos);
        texcoords.x() = texCoords & 0xFF;
        texcoords.y() = (texCoords >> 8) & 0xFF;

    }

    void setPosition(u32 pos) {
        const u16 x = pos & 0xFFFF;
        const u16 y = (pos >> 16) & 0xFFFF;
        position.x() = static_cast<GLshort>(Helpers::signExtend16(x, 11));
        position.y() = static_cast<GLshort>(Helpers::signExtend16(y, 11));
    }
};

class GPU_GL final : public GPU {
  public:
    GPU_GL(Scheduler::Scheduler& scheduler);
    virtual ~GPU_GL();

    void reset() override;
    void init();

    OpenGL::Texture& getTexture();

    void setupDrawEnvironment();
    void render();
    void vblank();

    bool inVblank = false;
    bool updateScreen = false;

  private:
    void maybeRender(size_t count) {
        if (verts.size() + count >= vboSize) {
            batchRender();
        }
    }

    template <class... Args>
    void addVertex(Args&&... values) {
        verts.emplace_back(std::forward<Args>(values)...);
        vertCount++;
    }

    void batchRender();

    void drawCommand() override;
    void internalCommand(u32 value) override;
    void transferToVram() override;
    void transferToCpu() override;
    void TransferVramToVram() override;

    void setDrawMode(u32 value) override;
    void setTextureWindow(u32 value) override;
    void setDrawOffset(u32 value) override;
    void setDrawAreaTopLeft(u32 value) override;
    void setDrawAreaBottomRight(u32 value) override;
    void setMaskBitSetting(u32 value) override;

    void updateScissorBox() const;
    void updateDrawAreaScissor();
    void syncSampleTexture();
    void fillRect();

    template <Polygon polygon, Shading shading, Transparency transparency>
    void drawPolygon();

    template <Rectsize size, Transparency transparency, Shading shading = Shading::None>
    void drawRect();

    template <Shading shading, Transparency transparency>
    void drawLine();

    void blankDraw();

    void hblankEvent();
    void scanlineEvent();

    std::vector<Vertex> verts;
    size_t vertCount = 0;

    OpenGL::VertexBuffer vbo;
    OpenGL::VertexArray vao;
    OpenGL::Framebuffer vramFBO;
    OpenGL::Framebuffer blankFBO;
    OpenGL::Texture vramTex;
    OpenGL::Texture blankTex;
    OpenGL::Texture sampleTex;

    Rect<int> scissorBox;

    OpenGL::ShaderProgram shaders;
    GLint uniformTextureLocation = 0;
    GLint uniformTextureWindow = 0;
    GLint uniformDrawOffsetLocation = 0;
    GLint uniformBlendFactors = 0;
    GLint uniformOpaqueBlendFactors = 0;
    OpenGL::vec2 blendFactors;
    Transparency lastTransparency = Transparency::Opaque;
    int lastBlendMode = -1;

    template <Transparency transparency>
    void setTransparency();

    void setBlendFactors(float source, float destination);
    void setBlendModeTexpage(u32 texpage);

    static constexpr int vboSize = 0x100000;
    bool syncSampleTex = false;
    bool updateDrawOffset = false;

    static constexpr u64 CYCLES_PER_HDRAW = 2560 / 1.57;
    static constexpr u64 CYCLES_PER_SCANLINE = 3413 / 1.57;  // NTSC
    static constexpr u64 SCANLINES_PER_VDRAW = 240;
    static constexpr u64 SCANLINES_PER_FRAME = 263;
    u64 lineCount = 0;
};

}  // namespace GPU

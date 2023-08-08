#pragma once
#include <vector>

#include "gpu.hpp"

namespace GPU {

struct Vertex {
    OpenGL::ivec2 position;
    u32 color;
    u16 texpage;
    u16 clut;
    u16 texcoords;

    Vertex() : position({0, 0}), color(0), texpage(0), clut(0), texcoords(0) {}
    Vertex(u32 pos, u32 color) : color(color) {
        setPosition(pos);
        texpage = 0x8000;
    }

    Vertex(u32 pos, u32 color, u16 texpage, u16 clut, u16 texcoords) : color(color), texpage(texpage), clut(clut), texcoords(texcoords) {
        setPosition(pos);
    }

    void setPosition(u32 pos) {
        const u16 x = pos & 0xFFFF;
        const u16 y = (pos >> 16) & 0xFFFF;
        position.x() = Helpers::signExtend16(x, 11);
        position.y() = Helpers::signExtend16(y, 11);
    }
};

class GPU_GL final : public GPU {
  public:
    GPU_GL();
    virtual ~GPU_GL();

    void reset() override;
    void init();

    OpenGL::Texture& getTexture();

    void setupDrawEnvironment();
    void render();
    void vblank();

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

    void updateScissorBox();
    void updateDrawAreaScissor();
    void syncSampleTexture();
    void fillRect();

    template <Polygon polygon, Shading shading, Transparency transparency>
    void drawPolygon();

    template <Rectsize size, Transparency transparency, Shading shading = Shading::None>
    void drawRect();

    void blankDraw();

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
    static constexpr int vboSize = 0x100000;
    bool syncSampleTex = false;
};

}  // namespace GPU

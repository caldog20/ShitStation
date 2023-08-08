#include "gpugl.hpp"

#include <algorithm>

namespace GPU {

static const char* vertShader = R"(
    #version 410 core
    layout (location = 0) in ivec2 inPos;
    layout (location = 1) in int inColor;
    layout (location = 2) in int inTexpage;
    layout (location = 3) in int inClut;
    layout (location = 4) in int inTexcoords;


    out vec4 outColor;
    flat out ivec2 texpageBase;
    flat out ivec2 clutBase;
    flat out int outTexcoords;
    flat out int texMode;

    uniform vec2 u_drawOffsets = vec2(0.5f, -0.5f);

    void main() {
        float x = float(inPos.x);
        float y = float(inPos.y);

       float xx = (x + u_drawOffsets.x) / 512.0f;
       float yy = (y + u_drawOffsets.y) / 256.0f;

        xx -= 1.0f;
        yy -= 1.0f;

        int r = (inColor >> 0) & 0xFF;
        int g = (inColor >> 8) & 0xFF;
        int b = (inColor >> 16) & 0xFF;

        if ((inTexpage & 0x8000) != 0) {
            texMode = 4;
        } else {
            texMode = (inTexpage >> 7) & 3;
            outTexcoords = inTexcoords;
            texpageBase = ivec2((inTexpage & 0xf) * 64, ((inTexpage >> 4) & 0x1) * 256);
            clutBase = ivec2((inClut & 0x3f) * 16, inClut >> 6);
        }

        gl_Position = vec4(xx, yy, 0.0f, 1.0f);
        outColor = vec4(float(r) / 255.0f, float(g) / 255.0f, float(b) / 255.0f, 1.0f);
    }

)";

static const char* fragShader = R"(
    #version 410 core
    in vec4 outColor;
    flat in ivec2 texpageBase;
    flat in ivec2 clutBase;
    flat in int outTexcoords;
    flat in int texMode;

    layout(location = 0, index = 0) out vec4 fragColor;
    uniform sampler2D u_sampleTex;

    void main() {
        if (texMode == 4) {
            fragColor = outColor;
            return;
        }


    }
)";

GPU_GL::GPU_GL() {}
GPU_GL::~GPU_GL() {}

void GPU_GL::reset() {
    GPU::reset();
    verts.clear();
    vertCount = 0;
    drawArea.left = 0;
    drawArea.top = 0;
    drawArea.right = VRAM_WIDTH;
    drawArea.bottom = VRAM_HEIGHT;
    updateDrawAreaScissor();
}

void GPU_GL::init() {
    shaders.build(vertShader, fragShader);

    vramFBO.create();
    vramFBO.bind();

    vramTex.create(GL_RGBA8, VRAM_WIDTH, VRAM_HEIGHT);
    vramTex.setFiltering(OpenGL::Linear);
    vramFBO.attachTexture(vramTex.handle());

    OpenGL::checkFramebufferStatus();

    OpenGL::setClearColor();
    OpenGL::clearColor();

    blankFBO.create();
    blankFBO.bind();
    blankTex.create(GL_RGBA8, VRAM_WIDTH, VRAM_HEIGHT);
    blankTex.setFiltering(OpenGL::Linear);
    blankFBO.attachTexture(blankTex.handle());
    OpenGL::setClearColor();
    OpenGL::clearColor();

    sampleTex.create(GL_RGBA8, VRAM_WIDTH, VRAM_HEIGHT);
    sampleTex.setFiltering(OpenGL::Linear);

    vao.create();
    vbo.createFixed(OpenGL::ArrayBuffer, vboSize, OpenGL::StreamDraw);

    vao.bind();
    vbo.bind();

    vao.setAttributeInt<int>(0, 2, sizeof(Vertex), offsetof(Vertex, position));
    vao.enableAttribute(0);

    vao.setAttributeInt<int>(1, 1, sizeof(Vertex), offsetof(Vertex, color));
    vao.enableAttribute(1);

    vao.setAttributeInt<int>(2, 1, sizeof(Vertex), offsetof(Vertex, texpage));
    vao.enableAttribute(2);

    vao.setAttributeInt<int>(3, 1, sizeof(Vertex), offsetof(Vertex, clut));
    vao.enableAttribute(3);

    vao.setAttributeInt<int>(4, 1, sizeof(Vertex), offsetof(Vertex, texcoords));
    vao.enableAttribute(4);

    //    shaders.use();
    //    uniformTextureLocation = shaders.getUniformLocation("u_vramTex");
    //    glUniform1i(uniformTextureLocation, 0);
    //    glUseProgram(0);

    OpenGL::bindDefaultTexture();
    OpenGL::bindDefaultFramebuffer();
}

OpenGL::Texture& GPU_GL::getTexture() {
    if (disableDisplay) return blankTex;
    return vramTex;
}

void GPU_GL::setupDrawEnvironment() {
    OpenGL::enableScissor();
    vramFBO.bind();
    vao.bind();
    vbo.bind();
    sampleTex.bind();
    OpenGL::setViewport(VRAM_WIDTH, VRAM_HEIGHT);
    shaders.use();
}

void GPU_GL::render() {
    if (syncSampleTex) {
        syncSampleTexture();
    }
    if (vertCount == 0) return;

    vbo.subData(verts.data(), vertCount);

    OpenGL::drawArrays(OpenGL::Triangles, 0, vertCount);

    verts.clear();
    vertCount = 0;
}

void GPU_GL::vblank() {
    vao.unbind();
    vbo.unbind();
    OpenGL::bindDefaultFramebuffer();
    OpenGL::bindDefaultTexture();
    OpenGL::disableScissor();
}

void GPU_GL::internalCommand(u32 value) {
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
        default: Log::warn("[GPU] GP0 Internal - Unhandled command: {:#02X}\n", command); return;
    }
    updateGPUStat();
}

void GPU_GL::drawCommand() {
    switch (command) {
        case 0x02: fillRect(); break;
        case 0xA0: {
            u16 x = args[1] & 0x3ff;
            u16 y = (args[1] >> 16) & 0x1ff;

            u16 w = args[2] & 0xffff;
            u16 h = args[2] >> 16;

            w = ((w - 1) & 0x3ff) + 1;
            h = ((h - 1) & 0x1ff) + 1;

            u32 size = ((w * h) + 1) & ~1;
            transferSize = size / 2;

            writeMode = Transfer;
            transferRect = {x, y, w, h};
            break;
        }
        case 0x20:
            drawPolygon<Polygon::Triangle, Shading::Flat, Transparency::Opaque>();
            break;
            //        case 0x22:
            //            drawPolygon<Polygon::Triangle, Shading::Flat, Transparency::Transparent>();
            //            break;
        case 0x28:
        case 0x29:
            drawPolygon<Polygon::Quad, Shading::Flat, Transparency::Opaque>();
            break;
            //
        case 0x30: drawPolygon<Polygon::Triangle, Shading::Gouraud, Transparency::Opaque>(); break;
        case 0x32:
            //            drawPolygon<Polygon::Triangle, Shading::Gouraud, Transparency::Transparent>();
            break;
        case 0x38:
        case 0x39: drawPolygon<Polygon::Quad, Shading::Gouraud, Transparency::Opaque>(); break;

        default: Log::debug("Unimplemented GP0 Command {:#02x}\n", command);
    }
}

void GPU_GL::batchRender() {}

void GPU_GL::blankDraw() {}

template <GPU::Polygon polygon, GPU::Shading shading, GPU::Transparency transparency>
void GPU_GL::drawPolygon() {
    using enum Shading;
    using enum Transparency;
    using enum Polygon;

    int vertCount;

    if constexpr (polygon == Triangle) vertCount = 3;
    if constexpr (polygon == Quad) vertCount = 6;

    maybeRender(vertCount);

    if constexpr (shading == Flat) {
        addVertex(args[1], args[0]);
        addVertex(args[2], args[0]);
        addVertex(args[3], args[0]);
        if constexpr (polygon == Quad) {
            addVertex(args[2], args[0]);
            addVertex(args[3], args[0]);
            addVertex(args[4], args[0]);
        }
    }

    if constexpr (shading == Gouraud) {
        addVertex(args[1], args[0]);
        addVertex(args[3], args[2]);
        addVertex(args[5], args[4]);
        if constexpr (polygon == Quad) {
            addVertex(args[3], args[2]);
            addVertex(args[5], args[4]);
            addVertex(args[7], args[6]);
        }
    }

    if constexpr (shading == TexBlendFlat || shading == RawTex) {
        u32 color = shading == TexBlendFlat ? args[0] : 0x808080;
        u16 texpage = (args[4] >> 16) & 0x3FFF;
        u16 clut = args[2] >> 16;
        addVertex(args[1], color, texpage, clut, args[2] & 0xffff);
        addVertex(args[3], color, texpage, clut, args[4] & 0xffff);
        addVertex(args[5], color, texpage, clut, args[6] & 0xffff);
        if constexpr (polygon == Quad) {
            addVertex(args[3], color, texpage, clut, args[4] & 0xffff);
            addVertex(args[5], color, texpage, clut, args[6] & 0xffff);
            addVertex(args[7], color, texpage, clut, args[8] & 0xffff);
        }
    }
}

template <GPU::Rectsize size, GPU::Transparency transparency, GPU::Shading shading>
void GPU_GL::drawRect() {}

void GPU_GL::updateScissorBox() { OpenGL::setScissor(scissorBox.x, scissorBox.y, scissorBox.w, scissorBox.h); }

void GPU_GL::updateDrawAreaScissor() {
    render();
    const int left = drawArea.left;
    const int width = std::max<int>(drawArea.right - drawArea.left + 1, 0);
    const int top = drawArea.top;
    const int height = std::max<int>(drawArea.bottom - drawArea.top + 1, 0);

    scissorBox = {left, top, width, height};
    updateScissorBox();
}

void GPU_GL::setTextureWindow(u32 value) {
    // 8 pixel steps - multiply by 8
    texWindow.xMask = value & 0x1F;
    texWindow.yMask = (value >> 5) & 0x1F;
    texWindow.x = (value >> 10) & 0x1F;
    texWindow.y = (value >> 15) & 0x1F;
}

void GPU_GL::setDrawOffset(u32 value) {
    u16 x = value & 0x7FF;
    u16 y = (value >> 11) & 0x7FF;

    drawOffset.x() = static_cast<s16>(x << 5) >> 5;
    drawOffset.y() = static_cast<s16>(y << 5) >> 5;
    const auto location = shaders.getUniformLocation("u_drawOffsets");
    glUniform2f(location, static_cast<float>(drawOffset.x()) + 0.5f, static_cast<float>(drawOffset.y()) - 0.5f);
}

void GPU_GL::setDrawAreaTopLeft(u32 value) {
    drawArea.top = (value >> 10) & 0x3FF;
    drawArea.left = value & 0x3FF;
    updateDrawAreaScissor();
}

void GPU_GL::setDrawAreaBottomRight(u32 value) {
    drawArea.bottom = (value >> 10) & 0x3FF;
    drawArea.right = value & 0x3FF;
    updateDrawAreaScissor();
}

void GPU_GL::setDrawMode(u32 value) {
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

void GPU_GL::setMaskBitSetting(u32 value) {
    setMaskBit = Helpers::isBitSet(value, 0);
    preserveMaskedPixels = Helpers::isBitSet(value, 1);
}

void GPU_GL::fillRect() {
    render();  // Render out remaining verts
    const u32 color = args[0] & 0xFFFFFF;
    float r = float(color & 0xff) / 255.0f;
    float g = float((color >> 8) & 0xff) / 255.0f;
    float b = float((color >> 16) & 0xff) / 255.0f;

    const u32 x = args[1] & 0xFFFF;
    const u32 y = (args[1] >> 16) & 0xFFFF;
    const u32 w = args[2] & 0xFFFF;
    const u32 h = (args[2] >> 16) & 0xFFFF;

    OpenGL::setClearColor(r, g, b, 1.0f);
    OpenGL::setScissor(x, y, w, h);
    OpenGL::clearColor();
    // Set scissor box back to draw area
    updateScissorBox();
}

void GPU_GL::transferToVram() {
    render();                          // Render out remaining verts
    OpenGL::bindDefaultFramebuffer();  // Unbind not to overwrite vram framebuffer
    vramTex.bind();                    // Texture to copy into
    glTexSubImage2D(
        GL_TEXTURE_2D, 0, transferRect.x, transferRect.y, transferRect.w, transferRect.h, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV,
        transferWriteBuffer.data()
    );
    syncSampleTex = true;
    setupDrawEnvironment();
    transferWriteBuffer.clear();
}

void GPU_GL::transferToCpu() {}

void GPU_GL::TransferVramToVram() {}

void GPU_GL::syncSampleTexture() {
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, VRAM_WIDTH, VRAM_HEIGHT);
    syncSampleTex = false;
}

}  // namespace GPU

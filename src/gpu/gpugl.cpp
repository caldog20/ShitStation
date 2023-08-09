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
    out vec2 outTexcoords;
    flat out int texMode;

    uniform vec2 u_drawOffsets = vec2(+0.5f, -0.5f);

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
            outTexcoords = vec2((inTexcoords & 0xff), (inTexcoords >> 8) & 0xff);
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
    in vec2 outTexcoords;
    flat in int texMode;

    layout(location = 0, index = 0) out vec4 fragColor;
    uniform sampler2D u_sampleTex;
    uniform ivec4 u_texWindow;
    vec4 cancelBlend = vec4(1.0, 1.0, 1.0, 0.0);

    int color5(vec4 color) {
        int r = int(floor(color.r * 31.0 + 0.5));
        int g = int(floor(color.g * 31.0 + 0.5));
        int b = int(floor(color.b * 31.0 + 0.5));
        int a = int(ceil(color.a));
        return r | (g << 5) | (b << 10) | (a << 15);
    }

    vec4 vramFetch(ivec2 coords) {
        coords &= ivec2(1023, 511);
        return texelFetch(u_sampleTex, coords, 0);
    }
    void main() {
//        fragColor = outColor;
//        return;
        if (texMode == 4) {
            fragColor = outColor;
            return;
        }

        ivec2 UV = ivec2(floor(outTexcoords + vec2(0.0001, 0.0001))) & ivec2(0xff);
        UV = (UV & u_texWindow.xy) | u_texWindow.zw;

        if (texMode == 0) {
            // 4-bit
            // Get texture coords
            ivec2 coords = ivec2(texpageBase.x + (UV.x >> 2), texpageBase.y + UV.y);
            // Get pixels from vram
            vec4 color = vramFetch(coords);
            // Convert to 5-bit color
            int index = color5(color);
            // Calculate clut index
            index = ((index >> (int(outTexcoords.x) % 4) * 4)) & 0xf;
            // Fetch color from clut
            fragColor = vramFetch(ivec2(clutBase.x + index, clutBase.y));
            // Discard if color is black for textures
            if (fragColor.rgb == vec3(0.0, 0.0, 0.0)) discard;
            // Blend vertex color with texture color
            // Color scale of 2
            vec4 finalColor = vec4(fragColor * outColor) / (128.0 / 255.0);
            finalColor.a = 1.0;
            fragColor = finalColor;
        } else if (texMode == 1) {
            ivec2 coords = ivec2(texpageBase.x + (UV.x >> 1), texpageBase.y + UV.y);
            vec4 color = vramFetch(coords);

            int index = color5(color);
            index = ((index >> (UV.x % 2) * 8)) & 0xff;

            fragColor = vramFetch(ivec2(clutBase.x + index, clutBase.y));

            if (fragColor.rgb == vec3(0.0, 0.0, 0.0)) discard;

            vec4 finalColor = vec4(fragColor * outColor) / (128.0 / 255.0);
            finalColor.a = 1.0;
            fragColor = finalColor;
        } else if (texMode == 2) {
            ivec2 coords = texpageBase + UV;
            fragColor = vramFetch(coords);
            if (fragColor.rgb == vec3(0.0, 0.0, 0.0)) discard;
            vec4 finalColor = vec4(fragColor * outColor) / (128.0 / 255.0);
            finalColor.a = 1.0;
            fragColor = finalColor;
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

    vao.setAttributeInt<GLshort>(0, 2, sizeof(Vertex), offsetof(Vertex, position));
    vao.enableAttribute(0);

    vao.setAttributeInt<int>(1, 1, sizeof(Vertex), offsetof(Vertex, color));
    vao.enableAttribute(1);

    vao.setAttributeInt<int>(2, 1, sizeof(Vertex), offsetof(Vertex, texpage));
    vao.enableAttribute(2);

    vao.setAttributeInt<int>(3, 1, sizeof(Vertex), offsetof(Vertex, clut));
    vao.enableAttribute(3);

    vao.setAttributeInt<int>(4, 1, sizeof(Vertex), offsetof(Vertex, texcoords));
    vao.enableAttribute(4);

    OpenGL::setPackAlignment(2);
    OpenGL::setUnpackAlignment(2);

    shaders.use();
    uniformTextureLocation = shaders.getUniformLocation("u_sampleTex");
    uniformTextureWindow = shaders.getUniformLocation("u_texWindow");
    shaders.getUniformLocation("u_drawOffsets");
    glUniform1i(uniformTextureLocation, 0);
    glUseProgram(0);

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
    glUniform1i(uniformTextureLocation, 0);
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
    //    Log::info("GP0 Command {:#02x}\n", command);
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
            size = size / 2;
            transferSize = size;
            writeMode = Transfer;
            transferRect = {x, y, w, h};
            break;
        }
        case 0xC0: transferToCpu(); break;
        case 0x20: drawPolygon<Polygon::Triangle, Shading::Flat, Transparency::Opaque>(); break;
        case 0x22: drawPolygon<Polygon::Triangle, Shading::Flat, Transparency::Transparent>(); break;
        case 0x24: drawPolygon<Polygon::Triangle, Shading::TexBlendFlat, Transparency::Opaque>(); break;
        case 0x25: drawPolygon<Polygon::Triangle, Shading::RawTex, Transparency::Opaque>(); break;
        case 0x26: drawPolygon<Polygon::Triangle, Shading::TexBlendFlat, Transparency::Transparent>(); break;
        case 0x27: drawPolygon<Polygon::Triangle, Shading::RawTex, Transparency::Transparent>(); break;
        case 0x28: drawPolygon<Polygon::Quad, Shading::Flat, Transparency::Opaque>(); break;
        case 0x29: drawPolygon<Polygon::Quad, Shading::Flat, Transparency::Opaque>(); break;
        case 0x2A: drawPolygon<Polygon::Quad, Shading::Flat, Transparency::Transparent>(); break;
        case 0x2C: drawPolygon<Polygon::Quad, Shading::TexBlendFlat, Transparency::Opaque>(); break;
        case 0x2D: drawPolygon<Polygon::Quad, Shading::RawTex, Transparency::Opaque>(); break;
        case 0x2E: drawPolygon<Polygon::Quad, Shading::TexBlendFlat, Transparency::Transparent>(); break;
        case 0x2F: drawPolygon<Polygon::Quad, Shading::RawTex, Transparency::Transparent>(); break;
        case 0x30: drawPolygon<Polygon::Triangle, Shading::Gouraud, Transparency::Opaque>(); break;
        case 0x32: drawPolygon<Polygon::Triangle, Shading::Gouraud, Transparency::Transparent>(); break;
        case 0x34: drawPolygon<Polygon::Triangle, Shading::TexBlendGouraud, Transparency::Opaque>(); break;
        case 0x35: drawPolygon<Polygon::Triangle, Shading::RawTexGouraud, Transparency::Opaque>(); break;
        case 0x36: drawPolygon<Polygon::Triangle, Shading::TexBlendGouraud, Transparency::Transparent>(); break;
        case 0x37: drawPolygon<Polygon::Triangle, Shading::RawTexGouraud, Transparency::Transparent>(); break;
        case 0x38: drawPolygon<Polygon::Quad, Shading::Gouraud, Transparency::Opaque>(); break;
        case 0x39: drawPolygon<Polygon::Quad, Shading::Gouraud, Transparency::Opaque>(); break;
        case 0x3A: drawPolygon<Polygon::Quad, Shading::Gouraud, Transparency::Transparent>(); break;
        case 0x3B: drawPolygon<Polygon::Quad, Shading::Gouraud, Transparency::Transparent>(); break;
        case 0x3C: drawPolygon<Polygon::Quad, Shading::TexBlendGouraud, Transparency::Opaque>(); break;
        case 0x3D: drawPolygon<Polygon::Quad, Shading::RawTexGouraud, Transparency::Opaque>(); break;
        case 0x3E: drawPolygon<Polygon::Quad, Shading::TexBlendGouraud, Transparency::Transparent>(); break;
        case 0x3F: drawPolygon<Polygon::Quad, Shading::RawTexGouraud, Transparency::Transparent>(); break;
        // No line drawing
        // 60 thru 7f Draw Rect
        case 0x60: drawRect<Rectsize::RectVariable, Transparency::Opaque>(); break;
        case 0x62: drawRect<Rectsize::RectVariable, Transparency::Transparent>(); break;
        case 0x64: drawRect<Rectsize::RectVariable, Transparency::Opaque, Shading::TexBlendFlat>(); break;
        case 0x65: drawRect<Rectsize::RectVariable, Transparency::Opaque, Shading::RawTex>(); break;
        case 0x66: drawRect<Rectsize::RectVariable, Transparency::Transparent, Shading::TexBlendFlat>(); break;
        case 0x67: drawRect<Rectsize::RectVariable, Transparency::Transparent, Shading::RawTex>(); break;
        case 0x68: drawRect<Rectsize::Rect1, Transparency::Opaque>(); break;
        case 0x70: drawRect<Rectsize::Rect8, Transparency::Opaque>(); break;
        case 0x74: drawRect<Rectsize::Rect8, Transparency::Opaque, Shading::TexBlendFlat>(); break;
        case 0x75: drawRect<Rectsize::Rect8, Transparency::Opaque, Shading::RawTex>(); break;
        case 0x7C: drawRect<Rectsize::Rect16, Transparency::Opaque, Shading::TexBlendFlat>(); break;
        case 0x7D: drawRect<Rectsize::Rect16, Transparency::Opaque, Shading::RawTex>(); break;
        case 0x7E: drawRect<Rectsize::Rect16, Transparency::Transparent, Shading::TexBlendFlat>(); break;
        case 0x7F: drawRect<Rectsize::Rect16, Transparency::Transparent, Shading::RawTex>(); break;

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
void GPU_GL::drawRect() {
    using enum Shading;
    using enum Transparency;
    using enum Rectsize;

    maybeRender(6);

    int width;
    int height;

    if constexpr (size == Rect1) {
        width = 1;
        height = 1;
    } else if constexpr (size == Rect8) {
        width = 8;
        height = 8;
    } else if constexpr (size == Rect16) {
        width = 16;
        height = 16;
    } else if constexpr (size == RectVariable) {
        width = args[2] & 0x3ff;
        height = (args[2] >> 16) & 0x1ff;
    }

    if constexpr (shading == None) {
        u32 color = args[0];
        u32 pos = args[1];

        int x = Helpers::signExtend16(pos, 11);
        int y = Helpers::signExtend16(pos, 5);

        addVertex(x, y, color);
        addVertex(x + width, y, color);
        addVertex(x + width, y + height, color);
        addVertex(x + width, y + height, color);
        addVertex(x, y + height, color);
        addVertex(x, y, color);
        return;
    }

    // textured rect
    u32 color = shading == TexBlendFlat ? args[0] : 0x808080;
    u32 pos = args[1];
    u32 clut = args[2] >> 16;
    u32 uv = args[2] & 0xffff;
    u16 texpage = rectTexpage;

    int x = Helpers::signExtend16(pos, 11);
    int y = Helpers::signExtend16(pos, 5);

    addVertex(x, y, color, texpage, clut, uv);
    addVertex(x + width, y, color, texpage, clut, uv);
    addVertex(x + width, y + height, color, texpage, clut, uv);
    addVertex(x + width, y + height, color, texpage, clut, uv);
    addVertex(x, y + height, color, texpage, clut, uv);
    addVertex(x, y, color, texpage, clut, uv);
}

void GPU_GL::updateScissorBox() const { OpenGL::setScissor(scissorBox.x, scissorBox.y, scissorBox.w, scissorBox.h); }

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
    render();
    texWindow.xMask = value & 0x1F * 8;
    texWindow.yMask = (value >> 5) & 0x1F * 8;
    texWindow.x = (value >> 10) & 0x1F * 8;
    texWindow.y = (value >> 15) & 0x1F * 8;
    glUniform4i(uniformTextureWindow, ~texWindow.xMask, ~texWindow.yMask, texWindow.x & texWindow.xMask, texWindow.yMask & texWindow.y);
}

void GPU_GL::setDrawOffset(u32 value) {
    render();
    u16 x = value & 0x7FF;
    u16 y = (value >> 11) & 0x7FF;

    drawOffset.x() = static_cast<s16>(x << 5) >> 5;
    drawOffset.y() = static_cast<s16>(y << 5) >> 5;
    glUniform2f(uniformDrawOffsetLocation, static_cast<float>(drawOffset.x()) + 0.5f, static_cast<float>(drawOffset.y()) - 0.5f);
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

void GPU_GL::transferToCpu() {
    render();
    readMode = GP0Mode::Transfer;

    u16 x = args[1] & 0x3ff;
    u16 y = (args[1] >> 16) & 0x1ff;

    u16 w = args[2] & 0xffff;
    u16 h = args[2] >> 16;

    w = ((w - 1) & 0x3ff) + 1;
    h = ((h - 1) & 0x1ff) + 1;

    // If the transfer size (width * height) is odd,
    // add 1 more transfer word to make it even
    u32 size = ((w * h) + 1) & ~1;
    transferSize = size / 2;
    transferIndex = 0;

    transferRect = {x, y, w, h};
    // Read vram data into buffer so cpu can read it
    transferReadBuffer.clear();
    glReadPixels(transferRect.x, transferRect.y, transferRect.w, transferRect.h, GL_RGBA, GL_UNSIGNED_SHORT_1_5_5_5_REV, transferReadBuffer.data());
}

void GPU_GL::TransferVramToVram() {}

void GPU_GL::syncSampleTexture() {
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, VRAM_WIDTH, VRAM_HEIGHT);
    syncSampleTex = false;
}

}  // namespace GPU

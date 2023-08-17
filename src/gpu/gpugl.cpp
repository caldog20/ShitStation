#include "gpugl.hpp"

#include <algorithm>
#include <utility>

#include "scheduler/scheduler.hpp"

namespace GPU {

static const char* vertShader = R"(
    #version 410 core
    layout (location = 0) in ivec2 inPos;
    layout (location = 1) in uint inColor;
    layout (location = 3) in int inClut;
    layout (location = 2) in int inTexpage;
    layout (location = 4) in ivec2 inUV;

    out vec4 vertexColor;
    out vec2 texCoords;
    flat out ivec2 clutBase;
    flat out ivec2 texpageBase;
    flat out int texMode;

    // We always apply a 0.5 offset in addition to the drawing offsets, to cover up OpenGL inaccuracies
    uniform vec2 u_drawOffsets = vec2(+0.5, -0.5);

    void main() {
        // Normalize coords to [0, 2]
        float x = float(inPos.x);
        float y = float(inPos.y);
        float xx = (x + u_drawOffsets.x) / 512.0;
        float yy = (y + u_drawOffsets.y) / 256;

        // Normalize to [-1, 1]
        xx -= 1.0;
        yy -= 1.0;

        float red = float(inColor & 0xffu);
        float green = float((inColor >> 8u) & 0xffu);
        float blue = float((inColor >> 16u) & 0xffu);
        vec3 color = vec3(red, green, blue);

        gl_Position = vec4(xx, yy, 1.0, 1.0);
        vertexColor = vec4(color / 255.0, 1.0);

        if ((inTexpage & 0x8000) != 0) { // Untextured primitive
            texMode = 4;
        } else {
            texMode = (inTexpage >> 7) & 3;
            texCoords = inUV;
            texpageBase = ivec2((inTexpage & 0xf) * 64, ((inTexpage >> 4) & 0x1) * 256);
            clutBase = ivec2((inClut & 0x3f) * 16, inClut >> 6);
        }
}

)";

static const char* fragShader = R"(
    #version 410 core

     in vec4 vertexColor;
     in vec2 texCoords;
     flat in ivec2 clutBase;
     flat in ivec2 texpageBase;
     flat in int texMode;

     // We use dual-source blending in order to emulate the fact that the GPU can enable blending per-pixel
     // FragColor: The colour of the pixel before alpha blending comes into play
     // BlendColor: Contains blending coefficients
     layout(location = 0, index = 0) out vec4 FragColor;
     layout(location = 0, index = 1) out vec4 BlendColor;

     // Tex window uniform format
     // x, y components: masks to & coords with
     // z, w components: masks to | coords with
     uniform ivec4 u_texWindow;
     uniform sampler2D u_sampleTex;
     uniform vec4 u_blendFactors;
     uniform vec4 u_opaqueBlendFactors = vec4(1.0, 1.0, 1.0, 0.0);

     int floatToU5(float f) {
         return int(floor(f * 31.0 + 0.5));
     }

     vec4 sampleVRAM(ivec2 coords) {
         coords &= ivec2(1023, 511); // Out-of-bounds VRAM accesses wrap
         return texelFetch(u_sampleTex, coords, 0);
     }

     int sample16(ivec2 coords) {
         vec4 colour = sampleVRAM(coords);
         int r = floatToU5(colour.r);
         int g = floatToU5(colour.g);
         int b = floatToU5(colour.b);
         int msb = int(ceil(colour.a)) << 15;
         return r | (g << 5) | (b << 10) | msb;
     }

     // Apply texture blending
         // Formula for RGB8 colours: col1 * col2 / 128
     vec4 texBlend(vec4 colour1, vec4 colour2) {
         vec4 ret = (colour1 * colour2) / (128.0 / 255.0);
         ret.a = 1.0;
         return ret;
     }

     void main() {
        if (texMode == 4) { // Untextured primitive
            FragColor = vertexColor;
            BlendColor = u_blendFactors;
            return;
        }

        // Fix up UVs and apply texture window
        ivec2 UV = ivec2(floor(texCoords + vec2(0.0001, 0.0001))) & ivec2(0xff);
        UV = (UV & u_texWindow.xy) | u_texWindow.zw;

        if (texMode == 0) { // 4bpp texture
            ivec2 texelCoord = ivec2(UV.x >> 2, UV.y) + texpageBase;

            int samp = sample16(texelCoord);
            int shift = (UV.x & 3) << 2;
            int clutIndex = (samp >> shift) & 0xf;

            ivec2 sampCoords = ivec2(clutBase.x + clutIndex, clutBase.y);
            FragColor = texelFetch(u_sampleTex, sampCoords, 0);

            if (FragColor.rgb == vec3(0.0, 0.0, 0.0)) discard;
            BlendColor = FragColor.a >= 0.5 ? u_blendFactors : u_opaqueBlendFactors;
            FragColor = texBlend(FragColor, vertexColor);
        } else if (texMode == 1) { // 8bpp texture
            ivec2 texelCoord = ivec2(UV.x >> 1, UV.y) + texpageBase;

            int samp = sample16(texelCoord);
            int shift = (UV.x & 1) << 3;
            int clutIndex = (samp >> shift) & 0xff;

            ivec2 sampCoords = ivec2(clutBase.x + clutIndex, clutBase.y);
            FragColor = texelFetch(u_sampleTex, sampCoords, 0);

            if (FragColor.rgb == vec3(0.0, 0.0, 0.0)) discard;
            BlendColor = FragColor.a >= 0.5 ? u_blendFactors : u_opaqueBlendFactors;
            FragColor = texBlend(FragColor, vertexColor);
        } else { // Texture depth 2 and 3 both indicate 16bpp textures
            ivec2 texelCoord = UV + texpageBase;
            FragColor = sampleVRAM(texelCoord);

            if (FragColor.rgb == vec3(0.0, 0.0, 0.0)) discard;
            FragColor = texBlend(FragColor, vertexColor);
            BlendColor = u_blendFactors;
        }
     }
)";

GPU_GL::GPU_GL(Scheduler::Scheduler& scheduler) : GPU(scheduler) {}

GPU_GL::~GPU_GL() {}

void GPU_GL::reset() {
    GPU::reset();
    verts.clear();
    verts.resize(vboSize);
    vertCount = 0;
    drawArea.left = 0;
    drawArea.top = 0;
    drawArea.right = VRAM_WIDTH;
    drawArea.bottom = VRAM_HEIGHT;
    updateDrawAreaScissor();

    //    scheduler.scheduleEvent(CYCLES_PER_HDRAW, [&]{
    //        hblankEvent();
    //    });
    //
    //    scheduler.scheduleEvent(CYCLES_PER_SCANLINE, [&]{
    //        scanlineEvent();
    //    });

    shaders.use();
    uniformTextureLocation = shaders.getUniformLocation("u_sampleTex");
    uniformTextureWindow = shaders.getUniformLocation("u_texWindow");
    uniformDrawOffsetLocation = shaders.getUniformLocation("u_drawOffsets");
    uniformBlendFactors = shaders.getUniformLocation("u_blendFactors");
    uniformOpaqueBlendFactors = shaders.getUniformLocation("u_opaqueBlendFactors");

    inVblank = false;
    lineCount = 0;

    lastBlendMode = -1;
    lastTransparency = Transparency::Opaque;
    setBlendFactors(0.0f, 0.0f);
    setDisplayEnable(false);
    setTextureWindow(0);
    setDrawOffset(0);
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

    vao.setAttributeInt<GLint>(0, 2, sizeof(Vertex), offsetof(Vertex, position));
    vao.enableAttribute(0);

    vao.setAttributeInt<GLuint>(1, 1, sizeof(Vertex), offsetof(Vertex, color));
    vao.enableAttribute(1);

    vao.setAttributeInt<GLushort>(2, 1, sizeof(Vertex), offsetof(Vertex, texpage));
    vao.enableAttribute(2);

    vao.setAttributeInt<GLushort>(3, 1, sizeof(Vertex), offsetof(Vertex, clut));
    vao.enableAttribute(3);

    vao.setAttributeInt<GLushort>(4, 2, sizeof(Vertex), offsetof(Vertex, texcoords));
    vao.enableAttribute(4);

    OpenGL::setPackAlignment(2);
    OpenGL::setUnpackAlignment(2);

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
    if (vertCount > 0) {
        if (syncSampleTex) {
            syncSampleTexture();
        }

        vbo.subData(verts.data(), vertCount);

        if (lastBlendMode == 2) {
            glBlendEquation(GL_FUNC_ADD);
            setBlendFactors(0.0, 1.0);
            OpenGL::drawArrays(OpenGL::Triangles, 0, vertCount);

            glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
            setBlendFactors(1.0, 1.0);
            glUniform4f(uniformOpaqueBlendFactors, 0.0, 0.0, 0.0, 1.0);
            OpenGL::drawArrays(OpenGL::Triangles, 0, vertCount);
            glUniform4f(uniformOpaqueBlendFactors, 1.0, 1.0, 1.0, 0.0);
        } else {
            OpenGL::drawArrays(OpenGL::Triangles, 0, vertCount);
        }
        verts.clear();
        vertCount = 0;
    }
}

void GPU_GL::vblank() {
    render();
    OpenGL::disableScissor();
    if (lastTransparency == Transparency::Transparent) {
        lastTransparency = Transparency::Opaque;
        lastBlendMode = -1;
        OpenGL::disableBlend();
    }

    vao.unbind();
    vbo.unbind();
    OpenGL::bindDefaultFramebuffer();
    OpenGL::bindDefaultTexture();
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

    const auto prepVramTransfer = [&] {
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
    };

    switch (command) {
        case 0x01:
            // Flush Tex cache
            render();
            syncSampleTex = true;
            break;
        case 0x02: fillRect(); break;
        case 0x80: TransferVramToVram(); break;
        case 0xA0: prepVramTransfer(); break;
        case 0xC0: transferToCpu(); break;
        case 0x20: drawPolygon<Polygon::Triangle, Shading::Flat, Transparency::Opaque>(); break;
        case 0x22: drawPolygon<Polygon::Triangle, Shading::Flat, Transparency::Transparent>(); break;
        case 0x24: drawPolygon<Polygon::Triangle, Shading::TexBlendFlat, Transparency::Opaque>(); break;
        case 0x25: drawPolygon<Polygon::Triangle, Shading::RawTex, Transparency::Opaque>(); break;
        case 0x26: drawPolygon<Polygon::Triangle, Shading::TexBlendFlat, Transparency::Transparent>(); break;
        case 0x27: drawPolygon<Polygon::Triangle, Shading::RawTex, Transparency::Transparent>(); break;
        case 0x28:
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
        case 0x38:
        case 0x39: drawPolygon<Polygon::Quad, Shading::Gouraud, Transparency::Opaque>(); break;
        case 0x3A:
        case 0x3B: drawPolygon<Polygon::Quad, Shading::Gouraud, Transparency::Transparent>(); break;
        case 0x3C: drawPolygon<Polygon::Quad, Shading::TexBlendGouraud, Transparency::Opaque>(); break;
        case 0x3D: drawPolygon<Polygon::Quad, Shading::RawTexGouraud, Transparency::Opaque>(); break;
        case 0x3E: drawPolygon<Polygon::Quad, Shading::TexBlendGouraud, Transparency::Transparent>(); break;
        case 0x3F: drawPolygon<Polygon::Quad, Shading::RawTexGouraud, Transparency::Transparent>(); break;
        case 0x40: drawLine<Shading::Flat, Transparency::Opaque>(); break;
        case 0x42: drawLine<Shading::Flat, Transparency::Transparent>(); break;
        case 0x50: drawLine<Shading::Gouraud, Transparency::Opaque>(); break;
        case 0x52: drawLine<Shading::Gouraud, Transparency::Transparent>(); break;
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

    setTransparency<transparency>();

    if constexpr (shading == Flat) {
        if constexpr (transparency == Transparent) {
            setBlendModeTexpage(rectTexpage);
        }
        addVertex(args[1], args[0]);
        addVertex(args[2], args[0]);
        addVertex(args[3], args[0]);
        if constexpr (polygon == Quad) {
            addVertex(args[2], args[0]);
            addVertex(args[3], args[0]);
            addVertex(args[4], args[0]);
        }
        return;
    }

    if constexpr (shading == Gouraud) {
        if constexpr (transparency == Transparent) {
            setBlendModeTexpage(rectTexpage);
        }
        addVertex(args[1], args[0]);
        addVertex(args[3], args[2]);
        addVertex(args[5], args[4]);
        if constexpr (polygon == Quad) {
            addVertex(args[3], args[2]);
            addVertex(args[5], args[4]);
            addVertex(args[7], args[6]);
        }
        return;
    }

    if constexpr (shading == TexBlendFlat || shading == RawTex) {
        if constexpr (transparency == Transparent) {
            setBlendModeTexpage(args[4] >> 16);
        }
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
    } else {
        if constexpr (transparency == Transparent) {
            setBlendModeTexpage(args[5] >> 16);
        }
        u32 clut = args[2] >> 16;
        u16 texpage = (args[5] >> 16) & 0x3FFF;
        if constexpr (shading == TexBlendGouraud) {
            addVertex(args[1], args[0], texpage, clut, args[2] & 0xffff);
            addVertex(args[4], args[3], texpage, clut, args[5] & 0xffff);
            addVertex(args[7], args[6], texpage, clut, args[8] & 0xffff);
            if constexpr (polygon == Quad) {
                addVertex(args[4], args[3], texpage, clut, args[5] & 0xffff);
                addVertex(args[7], args[6], texpage, clut, args[8] & 0xffff);
                addVertex(args[10], args[9], texpage, clut, args[11] & 0xffff);
            }
        } else {
            const u32 color = 0x808080;
            addVertex(args[1], color, texpage, clut, args[2] & 0xffff);
            addVertex(args[4], color, texpage, clut, args[5] & 0xffff);
            addVertex(args[7], color, texpage, clut, args[8] & 0xffff);
            if constexpr (polygon == Quad) {
                addVertex(args[4], color, texpage, clut, args[5] & 0xffff);
                addVertex(args[7], color, texpage, clut, args[8] & 0xffff);
                addVertex(args[10], color, texpage, clut, args[11] & 0xffff);
            }
        }
    }
}

template <GPU::Rectsize size, GPU::Transparency transparency, GPU::Shading shading>
void GPU_GL::drawRect() {
    using enum Shading;
    using enum Transparency;
    using enum Rectsize;

    maybeRender(6);

    setTransparency<transparency>();
    if constexpr (transparency == Transparent) {
        setBlendModeTexpage(rectTexpage);
    }

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
        width = args[3] & 0x3ff;
        height = (args[3] >> 16) & 0x1ff;
    }

    if constexpr (shading == None) {
        u32 color = args[0];
        u32 pos = args[1];

        const int x = int(pos) << 21 >> 21;
        const int y = int(pos) << 5 >> 21;

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
    u32 u = uv & 0xFF;
    u32 v = uv >> 8;
    u16 texpage = rectTexpage;

    //    int x = Helpers::signExtend16(pos, 11);
    //    int y = Helpers::signExtend16(pos, 5);

    const int x = int(pos) << 21 >> 21;
    const int y = int(pos) << 5 >> 21;

    addVertex(x, y, color, texpage, clut, u, v);
    addVertex(x + width, y, color, texpage, clut, u + width, v);
    addVertex(x + width, y + height, color, texpage, clut, u + width, v + height);
    addVertex(x + width, y + height, color, texpage, clut, u + width, v + height);
    addVertex(x, y + height, color, texpage, clut, u, v + height);
    addVertex(x, y, color, texpage, clut, u, v);
}

template <GPU::Shading shading, GPU::Transparency transparency>
void GPU_GL::drawLine() {
    // This line drawing algorithm was taken from the old opengl gpu on pcsx-redux
    // Since I didn't know how to draw lines
    using enum Shading;
    using enum Transparency;

    maybeRender(6);

    setTransparency<transparency>();
    if constexpr (transparency == Transparent) {
        setBlendModeTexpage(rectTexpage);
    }

    Point p1;
    Point p2;

    if constexpr (shading == Flat) {
        p1 = {args[1], args[0]};
        p2 = {args[2], args[0]};
    } else {
        p1 = {args[1], args[0]};
        p2 = {args[3], args[2]};
    }

    const auto getPos = [](const Point& point) -> std::pair<s32, s32> {
        s32 x = s32(point.pos) << 21 >> 21;
        s32 y = s32(point.pos) << 5 >> 21;
        return {x, y};
    };

    auto [x1, y1] = getPos(p1);
    auto [x2, y2] = getPos(p2);

    const s32 dx = x2 - x1;
    const s32 dy = y2 - y1;

    const auto absDx = std::abs(dx);
    const auto absDy = std::abs(dy);

    // Both vertices coincide, render 1x1 rectangle with the colour and coords of v1
    if (dx == 0 && dy == 0) {
        verts.emplace_back(x1, y1, p1.color);
        verts.emplace_back(x1 + 1, y1, p1.color);
        verts.emplace_back(x1 + 1, y1 + 1, p1.color);

        verts.emplace_back(x1 + 1, y1 + 1, p1.color);
        verts.emplace_back(x1, y1 + 1, p1.color);
        verts.emplace_back(x1, y1, p1.color);
    } else {
        int xOffset, yOffset;
        if (absDx > absDy) {  // x-major line
            xOffset = 0;
            yOffset = 1;

            // Align line depending on whether dx is positive or not
            dx > 0 ? x2++ : x1++;
        } else {  // y-major line
            xOffset = 1;
            yOffset = 0;

            // Align line depending on whether dy is positive or not
            dy > 0 ? y2++ : y1++;
        }

        verts.emplace_back(x1, y1, p1.color);
        verts.emplace_back(x2, y2, p2.color);
        verts.emplace_back(x2 + xOffset, y2 + yOffset, p2.color);

        verts.emplace_back(x2 + xOffset, y2 + yOffset, p2.color);
        verts.emplace_back(x1 + xOffset, y1 + yOffset, p1.color);
        verts.emplace_back(x1, y1, p1.color);
    }
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
    texWindow.xMask = (value & 0x1F) * 8;
    texWindow.yMask = ((value >> 5) & 0x1F) * 8;
    texWindow.x = ((value >> 10) & 0x1F) * 8;
    texWindow.y = ((value >> 15) & 0x1F) * 8;
    glUniform4i(uniformTextureWindow, ~texWindow.xMask, ~texWindow.yMask, texWindow.x & texWindow.xMask, texWindow.yMask & texWindow.y);
}

void GPU_GL::setDrawOffset(u32 value) {
    render();

    const auto x = (s32)value << 21 >> 21;
    const auto y = (s32)value << 10 >> 21;

    drawOffset.x() = x;
    drawOffset.y() = y;

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

void GPU_GL::TransferVramToVram() {
    render();
    OpenGL::disableScissor();

    u32 src = args[1];
    u32 dst = args[2];
    u32 res = args[3];

    u32 srcX = src & 0x3ff;
    u32 srcY = (src >> 16) & 0x1ff;
    u32 dstX = dst & 0x3ff;
    u32 dstY = (dst >> 16) & 0x1ff;

    u32 width = res & 0xFFFF;
    u32 height = res >> 16;

    width = ((width - 1) & 0x3ff) + 1;
    height = ((height - 1) & 0x1ff) + 1;

    glBlitFramebuffer(srcX, srcY, srcX + width, srcY + height, dstX, dstY, dstX + width, dstY + height, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    OpenGL::enableScissor();
}

void GPU_GL::syncSampleTexture() {
    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, VRAM_WIDTH, VRAM_HEIGHT);
    syncSampleTex = false;
}

void GPU_GL::hblankEvent() {
    // timers step hblank
    //    scheduler.scheduleEvent(CYCLES_PER_HDRAW, [&]{
    //        hblankEvent();
    //    });
}

void GPU_GL::scanlineEvent() {
    //    lineCount++;
    //
    //    if (lineCount < SCANLINES_PER_VDRAW) {
    //       if (lineCount & 1) {
    //            gpustat |= 1 << 31;
    //       } else {
    //            gpustat &= ~(1 << 31);
    //       }
    //    } else {
    //        gpustat |= 1 << 31;
    //    }
    //
    //    if (lineCount == SCANLINES_PER_VDRAW) {
    //        //vblank
    //        scheduler.scheduleInterrupt(1, Bus::IRQ::VBLANK);
    //        inVblank = true;
    //        updateScreen = true;
    //    } else if (lineCount == SCANLINES_PER_FRAME) {
    //        // vblank end
    //        inVblank = false;
    //        lineCount = 0;
    //    }
    //
    //    scheduler.scheduleEvent(CYCLES_PER_SCANLINE, [&]{
    //        scanlineEvent();
    //    });
}

template <GPU::Transparency transparency>
void GPU_GL::setTransparency() {
    if (lastTransparency != transparency) {
        render();
        if constexpr (transparency == Transparency::Opaque) {
            lastBlendMode = -1;
            OpenGL::disableBlend();
        } else {
            OpenGL::enableBlend();
        }
        lastTransparency = transparency;
    }
}

void GPU_GL::setBlendFactors(float source, float destination) {
    if (blendFactors.x() != source || blendFactors.y() != destination) {
        blendFactors.x() = source;
        blendFactors.y() = destination;

        glUniform4f(uniformBlendFactors, source, source, source, destination);
    }
}

void GPU_GL::setBlendModeTexpage(u32 texpage) {
    const auto blendMode = (texpage >> 5) & 3;
    if (lastBlendMode != blendMode) {
        render();
        lastBlendMode = blendMode;
        glBlendFuncSeparate(GL_SRC1_COLOR, GL_SRC1_ALPHA, GL_ONE, GL_ZERO);

        switch (blendMode) {
            case 0:
                glBlendEquation(GL_FUNC_ADD);
                setBlendFactors(0.5, 0.5);
                break;
            case 1:
                glBlendEquation(GL_FUNC_ADD);
                setBlendFactors(1.0, 0.0);
                break;
            case 2: break;
            case 3:
                glBlendEquation(GL_FUNC_ADD);
                setBlendFactors(0.25, 1.0);
                break;
        }
    }
}

}  // namespace GPU

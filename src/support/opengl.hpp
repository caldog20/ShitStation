#pragma once
#include <cassert>
#include <cstdio>
#include <string>
#include <type_traits>
#include <utility>

#include "glad/gl.h"

namespace OpenGL {

// Workaround for using static_assert inside constexpr if
// https://stackoverflow.com/questions/53945490/how-to-assert-that-a-constexpr-if-else-clause-never-happen
// I tried to rewrite most of this on my own to learn but alot was taken from Peach (@wheremyfoodat) and from PCSX-Redux
template <class...>
constexpr std::false_type AlwaysFalse{};

template <class T>
concept Scalar = std::is_scalar<T>::value;
template <Scalar T, size_t size>
struct Vector {
  public:
    T& x() { return m_storage[0]; }
    T& y() { return m_storage[1]; }
    T& z() {
        static_assert(size >= 3, "Invalid Vector Access: Vector size < 3");
        return m_storage[2];
    }
    T& w() {
        static_assert(size >= 4, "Invalid Vector Access: Vector size < 4");
        return m_storage[3];
    }

    T& r() { return m_storage[0]; }
    T& g() { return m_storage[1]; }
    T& b() {
        static_assert(size >= 3, "Invalid Vector Access: Vector size < 3");
        return m_storage[2];
    }
    T& a() {
        static_assert(size >= 4, "Invalid Vector Access: Vector size < 4");
        return m_storage[3];
    }

    Vector(std::array<T, size> list) { std::copy(list.begin(), list.end(), &m_storage[0]); }

    Vector() {}

  private:
    static_assert(size >= 2 && size <= 4);
    T m_storage[size];
};

using vec2 = Vector<GLfloat, 2>;
using vec3 = Vector<GLfloat, 3>;
using vec4 = Vector<GLfloat, 4>;
using ivec2 = Vector<GLint, 2>;
using ivec3 = Vector<GLint, 3>;

enum DrawType {
    Triangles = GL_TRIANGLES,
    TriangleStrip = GL_TRIANGLE_STRIP,
    TriangleFan = GL_TRIANGLE_FAN,
};

enum FB {
    Default = GL_FRAMEBUFFER,
    Read = GL_READ_FRAMEBUFFER,
    Draw = GL_DRAW_FRAMEBUFFER,
};

enum Filtering {
    Linear = GL_LINEAR,
    Nearest = GL_NEAREST,
};

enum BufferTarget {
    ArrayBuffer = GL_ARRAY_BUFFER,
    ElementArrayBuffer = GL_ELEMENT_ARRAY_BUFFER,
};

enum BufferUsage {
    StaticDraw = GL_STATIC_DRAW,
    DynamicDraw = GL_DYNAMIC_DRAW,
    StreamDraw = GL_STREAM_DRAW,
};

enum class ShaderType {
    Vertex = GL_VERTEX_SHADER,
    Fragment = GL_FRAGMENT_SHADER,
    Program = GL_PROGRAM,
};

static void drawArrays(DrawType type, GLint first, GLsizei count) { glDrawArrays(static_cast<GLenum>(type), first, count); }

static void clear(GLbitfield mask) { glClear(mask); }
static void setClearColor(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f) { glClearColor(r, g, b, a); }
static void clearColor() { clear(GL_COLOR_BUFFER_BIT); }
static void setViewport(GLint x, GLint y, GLsizei width, GLsizei height) { glViewport(x, y, width, height); }
static void setViewport(GLsizei width, GLsizei height) { setViewport(0, 0, width, height); }
static void bindDefaultFramebuffer() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
static void bindDefaultTexture() { glBindTexture(GL_TEXTURE_2D, 0); }
static void enableBlend() { glEnable(GL_BLEND); }
static void disableBlend() { glDisable(GL_BLEND); }
static void enableScissor() { glEnable(GL_SCISSOR_TEST); }
static void disableScissor() { glDisable(GL_SCISSOR_TEST); }
static void setScissor(GLsizei width, GLsizei height) { glScissor(0, 0, width, height); }
static void setScissor(GLsizei x, GLsizei y, GLsizei width, GLsizei height) { glScissor(x, y, width, height); }

template <FB type = Default>
static void checkFramebufferStatus() {
    if (glCheckFramebufferStatus(static_cast<GLenum>(type)) != GL_FRAMEBUFFER_COMPLETE) {
        throw std::runtime_error("Framebuffer setup incomplete.");
    }
}

class Framebuffer {
  public:
    Framebuffer() {}
    ~Framebuffer() {
        if (m_handle) glDeleteFramebuffers(GL_FRAMEBUFFER, &m_handle);
    }

    void create() { glGenFramebuffers(1, &m_handle); }

    void attachTexture(GLuint texture) { glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0); }

    template <FB type = Default>
    void bind() {
        glBindFramebuffer(static_cast<GLenum>(type), m_handle);
    }

    void unbind() { bindDefaultFramebuffer(); }

    GLuint handle() { return m_handle; }

  private:
    GLuint m_handle;
};

// Maybe fix later to support different bindings?
struct Texture {
    Texture() {}

    ~Texture() {
        if (m_handle) glDeleteTextures(1, &m_handle);
    }

    void create(GLenum internalFormat, GLsizei width, GLsizei height) {
        glGenTextures(1, &m_handle);
        bind();
        glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
    }

    void set(GLuint handle) { m_handle = handle; }

    void bind() { glBindTexture(GL_TEXTURE_2D, m_handle); }
    void unbind() { glBindTexture(GL_TEXTURE_2D, 0); }
    GLuint handle() { return m_handle; }

    void setFiltering(Filtering filter) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(static_cast<GLenum>(filter)));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(static_cast<GLenum>(filter)));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    void createMipMap() { glGenerateMipmap(GL_TEXTURE_2D); }

    GLuint m_handle = 0;
};

struct VertexBuffer {
    VertexBuffer() {}

    ~VertexBuffer() {
        if (m_handle) glDeleteBuffers(1, &m_handle);
    }

    [[nodiscard]] bool isCreated() const { return m_handle != 0; }

    void create(BufferTarget target) {
        glGenBuffers(1, &m_handle);
        m_target = target;
    }

    void createFixed(BufferTarget target, GLsizei size, BufferUsage usage) {
        glGenBuffers(1, &m_handle);
        m_target = target;
        bind();
        glBufferData(static_cast<GLenum>(m_target), size, nullptr, static_cast<GLenum>(usage));
    }

    void bind() { glBindBuffer(static_cast<GLenum>(m_target), m_handle); }
    void unbind() { glBindBuffer(static_cast<GLenum>(m_target), 0); }

    template <typename T>
    void data(T* data, int count, BufferUsage usage) {
        glBufferData(static_cast<GLenum>(m_target), sizeof(T) * count, data, static_cast<GLenum>(usage));
    }

    template <typename T>
    void subData(T* data, int count, int offset = 0) {
        glBufferSubData(static_cast<GLenum>(m_target), offset, sizeof(T) * count, data);
    }

    GLuint handle() { return m_handle; }

    GLuint m_handle = 0;
    BufferTarget m_target;
};

struct VertexArray {
    GLuint m_handle = 0;

    VertexArray() {}

    ~VertexArray() {
        if (m_handle) glDeleteVertexArrays(1, &m_handle);
    }

    [[nodiscard]] bool isCreated() const { return m_handle != 0; }
    void create() { glGenVertexArrays(1, &m_handle); }
    void bind() { glBindVertexArray(m_handle); }
    void unbind() { glBindVertexArray(0); }
    GLuint handle() const { return m_handle; }

    template <typename T>
    void setAttributeInt(GLuint index, GLint size, GLsizei stride, const void* pointer) {
        if constexpr (std::is_same<T, GLbyte>()) {
            glVertexAttribIPointer(index, size, GL_BYTE, stride, pointer);
        } else if constexpr (std::is_same<T, GLubyte>()) {
            glVertexAttribIPointer(index, size, GL_UNSIGNED_BYTE, stride, pointer);
        } else if constexpr (std::is_same<T, GLshort>()) {
            glVertexAttribIPointer(index, size, GL_SHORT, stride, pointer);
        } else if constexpr (std::is_same<T, GLushort>()) {
            glVertexAttribIPointer(index, size, GL_UNSIGNED_SHORT, stride, pointer);
        } else if constexpr (std::is_same<T, GLint>()) {
            glVertexAttribIPointer(index, size, GL_INT, stride, pointer);
        } else if constexpr (std::is_same<T, GLuint>()) {
            glVertexAttribIPointer(index, size, GL_UNSIGNED_INT, stride, pointer);
        } else {
            static_assert(AlwaysFalse<T>, "Unimplemented type for OpenGL::setAttributeInt");
        }
    }

    template <typename T>
    void setAttributeFloat(GLuint index, GLint size, GLsizei stride, const void* pointer, bool normalized = GL_FALSE) {
        if constexpr (std::is_same<T, GLfloat>()) {
            glVertexAttribPointer(index, size, GL_FLOAT, normalized, stride, pointer);
        } else if constexpr (std::is_same<T, GLbyte>()) {
            glVertexAttribPointer(index, size, GL_BYTE, normalized, stride, pointer);
        } else if constexpr (std::is_same<T, GLubyte>()) {
            glVertexAttribPointer(index, size, GL_UNSIGNED_BYTE, normalized, stride, pointer);
        } else if constexpr (std::is_same<T, GLshort>()) {
            glVertexAttribPointer(index, size, GL_SHORT, normalized, stride, pointer);
        } else if constexpr (std::is_same<T, GLushort>()) {
            glVertexAttribPointer(index, size, GL_UNSIGNED_SHORT, normalized, stride, pointer);
        } else if constexpr (std::is_same<T, GLint>()) {
            glVertexAttribPointer(index, size, GL_INT, normalized, stride, pointer);
        } else if constexpr (std::is_same<T, GLuint>()) {
            glVertexAttribPointer(index, size, GL_UNSIGNED_INT, normalized, stride, pointer);
        } else {
            static_assert(AlwaysFalse<T>, "Unimplemented type for OpenGL::setAttributeFloat");
        }
    }

    template <typename T>
    void setAttributeFloat(GLuint index, GLint size, GLsizei stride, size_t offset, bool normalized = false) {
        setAttributeFloat<T>(index, size, stride, reinterpret_cast<const void*>(offset), normalized);
    }

    template <typename T>
    void setAttributeInt(GLuint index, GLint size, GLsizei stride, size_t offset) {
        setAttributeInt<T>(index, size, stride, reinterpret_cast<const void*>(offset));
    }

    void enableAttribute(GLuint index) { glEnableVertexAttribArray(index); }

    void disableAttribute(GLuint index) { glDisableVertexAttribArray(index); }
};

struct ShaderProgram {
    ShaderProgram() {}
    ShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource) { build(vertexShaderSource, fragmentShaderSource); }

    ~ShaderProgram() {
        if (m_program) glDeleteProgram(m_program);
    }

    void use() { glUseProgram(m_program); }

    GLuint handle() const { return m_program; }

    [[nodiscard]] bool isCreated() const { return m_program != 0; }

    void verify(GLuint shader, ShaderType type) {
        GLint result;
        GLint length;
        if (type == ShaderType::Program) {
            glGetProgramiv(shader, GL_LINK_STATUS, &result);
            if (result != GL_TRUE) {
                glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &length);
                char log[length];
                glGetProgramInfoLog(shader, length, &length, log);
                fprintf(stderr, "Error Linking Shader Program: %s\n", log);
                exit(-1);
            }
        } else {
            glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
            if (result != GL_TRUE) {
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
                char log[length];
                glGetShaderInfoLog(shader, length, &length, log);
                fprintf(stderr, "Error Compiling Shader: %s\n", log);
                exit(-1);
            }
        }
    }

    void build(std::string_view vertexSource, std::string_view fragmentSource) { build(vertexSource.data(), fragmentSource.data()); }

    void build(const char* vertexShaderSource, const char* fragmentShaderSource) {
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        verify(vertexShader, ShaderType::Vertex);

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        verify(fragmentShader, ShaderType::Fragment);

        m_program = glCreateProgram();
        glAttachShader(m_program, vertexShader);
        glAttachShader(m_program, fragmentShader);
        glLinkProgram(m_program);
        verify(m_program, ShaderType::Program);
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    int getUniformLocation(const char* name) { return glGetUniformLocation(m_program, name); }

    GLuint m_program = 0;
};

}  // namespace OpenGL

// #pragma once
// #include <glad/gl.h>
//
// #include <array>
// #include <cstdio>
// #include <filesystem>
// #include <fstream>
// #include <functional>
// #include <initializer_list>
// #include <optional>
// #include <sstream>
// #include <stdexcept>
// #include <string>
// #include <string_view>
// #include <type_traits>
// #include <utility>
//
// #include "magic_enum.hpp"
// #include "support/helpers.hpp"
//
//// OpenGL Helper stuff for psx related graphics
//
// namespace OpenGL {
//
//// https://stackoverflow.com/questions/53945490/how-to-assert-that-a-constexpr-if-else-clause-never-happen
// template <class...>
// constexpr std::false_type AlwaysFalse{};
//
// enum FB {
//     Default = GL_FRAMEBUFFER,
//     Read = GL_READ_FRAMEBUFFER,
//     Draw = GL_DRAW_FRAMEBUFFER,
// };
//
// enum Filtering {
//     Linear = GL_LINEAR,
//     Nearest = GL_NEAREST,
// };
//
// enum VB {
//     ArrayBuffer = GL_ARRAY_BUFFER,
//     IndexBuffer = GL_ELEMENT_ARRAY_BUFFER,
//
// };
//
// enum BufferUsage {
//     StreamDraw = GL_STREAM_DRAW,
//     DynamicDraw = GL_DYNAMIC_DRAW,
//     StaticDraw = GL_STATIC_DRAW,
// };
//
// enum DrawMode {
//     Trianges = GL_TRIANGLES,
//     TriangleStrip = GL_TRIANGLE_STRIP,
//     Quad = GL_QUADS,
// };
//
// template <DrawMode mode>
// void drawArrays(GLsizei first, GLsizei count) {
//     glDrawArrays(static_cast<GLenum>(mode), first, count);
// }
//
// static inline void bindDefaultFramebuffer() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
// static inline void bindDefaultTexture() { glBindTexture(GL_TEXTURE_2D, 0); }
// static inline void setclearColor(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f) { glClearColor(r, g, b, a); }
// static inline void clearColor() { glClear(GL_COLOR_BUFFER_BIT); }
// static inline void setPackAlignment(GLint param) { glPixelStorei(GL_PACK_ALIGNMENT, param); }
// static inline void setUnpackAlignment(GLint param) { glPixelStorei(GL_UNPACK_ALIGNMENT, param); }
//
// static inline void setScissor(GLsizei width, GLsizei height) { glScissor(0, 0, width, height); }
// static inline void setScissor(GLsizei x, GLsizei y, GLsizei width, GLsizei height) { glScissor(x, y, width, height); }
// static inline void enableScissor() { glEnable(GL_SCISSOR_TEST); }
// static inline void disableScissor() { glDisable(GL_SCISSOR_TEST); }
// static inline void enableBlend() { glEnable(GL_BLEND); }
// static inline void disableBlend() { glDisable(GL_BLEND); }
//
// template <VB type>
// class VertexBuffer {
//   public:
//     VertexBuffer() {}
//     ~VertexBuffer() {
//         if (m_handle) glDeleteBuffers(1, &m_handle);
//     }
//
//     template <BufferUsage draw>
//     void gen(GLsizei size) {
//         glGenBuffers(1, &m_handle);
//         bind();
//         glBufferData(static_cast<GLenum>(type), size, nullptr, static_cast<GLenum>(draw));
//         m_size = size;
//     }
//
//     void gen() {
//         glGenBuffers(1, &m_handle);
//         bind();
//     }
//
//     GLuint get() { return m_handle; }
//     size_t size() { return m_size; }
//     void bind() { glBindBuffer(static_cast<GLenum>(type), m_handle); }
//     void unbind() { bindDefaultFramebuffer(); }
//
//     template <typename T, BufferUsage usage>
//     void bufferData(T* vertices, size_t count, GLintptr offset = 0) {
//         glBufferData(static_cast<GLenum>(type), sizeof(T) * count, vertices, static_cast<GLenum>(usage));
//     }
//
//     template <typename T>
//     void bufferSubData(T* vertices, size_t count, GLintptr offset = 0) {
//         glBufferSubData(static_cast<GLenum>(type), offset, sizeof(T) * count, vertices);
//     }
//
//   private:
//     GLuint m_handle;
//     GLsizei m_size;
// };
//
// class VertexArray {
//   public:
//     VertexArray() {}
//     ~VertexArray() {
//         if (m_handle) glDeleteVertexArrays(1, &m_handle);
//     }
//     void gen() { glGenVertexArrays(1, &m_handle); }
//     GLuint get() { return m_handle; }
//     void bind() { glBindVertexArray(m_handle); }
//     void unbind() { glBindVertexArray(0); }
//
//     template <GLuint index>
//     void setAttributeI(GLint size, GLenum type, GLsizei stride, const void* pointer) {
//         glVertexAttribIPointer(index, size, type, stride, pointer);
//     }
//
//     template <GLuint index>
//     void setAttribute(GLint size, GLenum type, bool normalized, GLsizei stride, const void* pointer) {
//         glVertexAttribPointer(index, size, type, normalized, stride, pointer);
//     }
//
//     // glVertexAttribPointer casts to float
//
//     template <typename T, GLuint index>
//     void setAttribute(GLint size, GLsizei stride, const void* pointer, bool normalized = false) {
//         if constexpr (std::is_same_v<T, GLfloat>) {
//             glVertexAttribPointer(index, size, GL_FLOAT, normalized, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLubyte>) {
//             glVertexAttribPointer(index, size, GL_UNSIGNED_BYTE, normalized, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLbyte>) {
//             glVertexAttribPointer(index, size, GL_BYTE, normalized, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLushort>) {
//             glVertexAttribPointer(index, size, GL_UNSIGNED_SHORT, normalized, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLshort>) {
//             glVertexAttribPointer(index, size, GL_SHORT, normalized, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLuint>) {
//             glVertexAttribPointer(index, size, GL_UNSIGNED_INT, normalized, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLint>) {
//             glVertexAttribPointer(index, size, GL_INT, normalized, stride, pointer);
//         } else {
//             static_assert(AlwaysFalse<T>, "Unsupported type for VertexArray::setAttribute<>()");
//         }
//     }
//
//     // glVertexAttribPointer does not cast to float
//     template <typename T, GLuint index>
//     void setAttributeI(GLint size, GLsizei stride, const void* pointer) {
//         if constexpr (std::is_same_v<T, GLfloat>) {
//             glVertexAttribIPointer(index, size, GL_FLOAT, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLubyte>) {
//             glVertexAttribIPointer(index, size, GL_UNSIGNED_BYTE, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLbyte>) {
//             glVertexAttribIPointer(index, size, GL_BYTE, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLushort>) {
//             glVertexAttribIPointer(index, size, GL_UNSIGNED_SHORT, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLshort>) {
//             glVertexAttribIPointer(index, size, GL_SHORT, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLuint>) {
//             glVertexAttribIPointer(index, size, GL_UNSIGNED_INT, stride, pointer);
//         } else if constexpr (std::is_same_v<T, GLint>) {
//             glVertexAttribIPointer(index, size, GL_INT, stride, pointer);
//         } else {
//             static_assert(AlwaysFalse<T>, "Unsupported type for VertexArray::setAttribute<>()");
//         }
//     }
//
//     template <GLuint index>
//     void enableAttribute() {
//         glEnableVertexAttribArray(index);
//     }
//
//   private:
//     GLuint m_handle;
// };
//
// template <FB type = Default>
// static void checkFramebufferStatus() {
//     if (glCheckFramebufferStatus(static_cast<GLenum>(type)) != GL_FRAMEBUFFER_COMPLETE) {
//         throw std::runtime_error("Framebuffer setup incomplete.");
//     }
// }
//
// class Framebuffer {
//   public:
//     Framebuffer() {}
//     ~Framebuffer() {
//         if (m_handle) glDeleteFramebuffers(GL_FRAMEBUFFER, &m_handle);
//     }
//
//     void gen() { glGenFramebuffers(1, &m_handle); }
//     void attachTexture(GLuint texture) { glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0); }
//
//     template <FB type = Default>
//     void bind() {
//         glBindFramebuffer(static_cast<GLenum>(type), m_handle);
//     }
//     void unbind() { bindDefaultFramebuffer(); }
//     GLuint get() { return m_handle; }
//
//   private:
//     GLuint m_handle;
// };
//
//// Maybe fix later to support different bindings?
// class Texture {
//   public:
//     Texture() {}
//     ~Texture() {
//         if (m_handle) glDeleteTextures(1, &m_handle);
//     }
//
//     void gen(GLenum internalFormat, GLsizei width, GLsizei height) {
//         glGenTextures(1, &m_handle);
//         bind();
//         glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
//     }
//
//     void bind() { glBindTexture(GL_TEXTURE_2D, m_handle); }
//     void unbind() { glBindTexture(GL_TEXTURE_2D, 0); }
//     GLuint get() { return m_handle; }
//
//     template <Filtering filter>
//     void setFiltering() {
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLenum>(filter));
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLenum>(filter));
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
//         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
//     }
//
//   private:
//     GLuint m_handle;
// };
//
// class Shader {
//     using fspath = std::filesystem::path;
//
//   public:
//     Shader() {}
//
//     ~Shader() {
//         if (m_program) glDeleteProgram(m_program);
//     }
//
//     void loadShadersFromString(const char* vertexShaderSource, const char* fragmentShaderSource) {
//         m_vertexShaderSource = vertexShaderSource;
//         m_fragmentShaderSource = fragmentShaderSource;
//         m_fromFile = false;
//         build();
//     }
//
//     void loadShadersFromFile(const fspath& vertexShaderPath, const fspath& fragmentShaderPath) {
//         m_vertexShaderPath = vertexShaderPath;
//         m_fragmentShaderPath = fragmentShaderPath;
//         m_fromFile = true;
//         std::ifstream vertexFile, fragmentFile;
//         vertexFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
//         fragmentFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
//
//         try {
//             vertexFile.open(m_vertexShaderPath);
//             std::stringstream vss;
//             vss << vertexFile.rdbuf();
//             vertexFile.close();
//             m_vertexShaderSource = vss.str();
//         } catch (std::ifstream::failure& error) {
//             Helpers::panic("Failed to load Vertex Shader: {} ", error.what());
//         }
//
//         try {
//             fragmentFile.open(m_fragmentShaderPath);
//             std::stringstream fss;
//             fss << fragmentFile.rdbuf();
//             fragmentFile.close();
//             m_fragmentShaderSource = fss.str();
//         } catch (std::ifstream::failure& error) {
//             Helpers::panic("Failed to load Fragment Shader: {}", error.what());
//         }
//
//         build();
//     }
//
//     void build() {
//         const char* vertexShaderSourceFinal = m_vertexShaderSource.c_str();
//         const char* fragmentShaderSourceFinal = m_fragmentShaderSource.c_str();
//
//         unsigned int vertexShader, fragmentShader;
//
//         vertexShader = glCreateShader(GL_VERTEX_SHADER);
//         glShaderSource(vertexShader, 1, &vertexShaderSourceFinal, NULL);
//         glCompileShader(vertexShader);
//         verifyShaderCompile(vertexShader, ShaderType::Vertex);
//
//         fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
//         glShaderSource(fragmentShader, 1, &fragmentShaderSourceFinal, NULL);
//         glCompileShader(fragmentShader);
//         verifyShaderCompile(fragmentShader, ShaderType::Fragment);
//
//         m_program = glCreateProgram();
//         glAttachShader(m_program, vertexShader);
//         glAttachShader(m_program, fragmentShader);
//         glLinkProgram(m_program);
//         verifyShaderCompile(m_program, ShaderType::Program);
//
//         glDeleteShader(vertexShader);
//         glDeleteShader(fragmentShader);
//     }
//
//     void activate() { glUseProgram(m_program); }
//
//     void deactivate() { glUseProgram(0); }
//
//     enum class ShaderType { Vertex, Fragment, Program };
//
//     void verifyShaderCompile(GLuint shader, ShaderType type) {
//         GLint result;
//         GLint length;
//         std::string log;
//         if (type == ShaderType::Program) {
//             glGetProgramiv(shader, GL_LINK_STATUS, &result);
//             if (result != GL_TRUE) {
//                 glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &length);
//                 log.resize(length);
//                 glGetProgramInfoLog(shader, length, &length, log.data());
//                 Helpers::panic("Error Linking Shader Program: {}\n", log);
//             }
//         } else {
//             glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
//             if (result != GL_TRUE) {
//                 glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
//                 log.resize(length);
//                 Helpers::panic("Error Compiling {} shader: {}\n", magic_enum::enum_name(type), log.data());
//             }
//         }
//     }
//
//     void recompile() {
//         glUseProgram(0);
//         glDeleteProgram(m_program);
//         if (m_fromFile) loadShadersFromFile(m_vertexShaderPath, m_fragmentShaderPath);
//         build();
//     };
//
//     GLuint ID() { return m_program; }
//
//   private:
//     fspath m_vertexShaderPath;
//     fspath m_fragmentShaderPath;
//     std::string m_vertexShaderSource;
//     std::string m_fragmentShaderSource;
//     GLuint m_program;
//     bool m_fromFile = false;
//     friend class ShaderUniform;
// };
//
//
//
// template <class T>
// concept Scalar = std::is_scalar<T>::value;
// template <Scalar T, size_t size>
// struct Vector {
//   public:
//     T& x() { return m_storage[0]; }
//     T& y() { return m_storage[1]; }
//     T& z() {
//         static_assert(size >= 3, "Invalid Vector Access: Vector size < 3");
//         return m_storage[2];
//     }
//     T& w() {
//         static_assert(size >= 4, "Invalid Vector Access: Vector size < 4");
//         return m_storage[3];
//     }
//
//     T& r() { return m_storage[0]; }
//     T& g() { return m_storage[1]; }
//     T& b() {
//         static_assert(size >= 3, "Invalid Vector Access: Vector size < 3");
//         return m_storage[2];
//     }
//     T& a() {
//         static_assert(size >= 4, "Invalid Vector Access: Vector size < 4");
//         return m_storage[3];
//     }
//
//     Vector(std::array<T, size> list) { std::copy(list.begin(), list.end(), &m_storage[0]); }
//
//     Vector() {}
//
//   private:
//     static_assert(size >= 2 && size <= 4);
//     T m_storage[size];
// };
//
// using vec2 = Vector<GLfloat, 2>;
// using vec3 = Vector<GLfloat, 3>;
//
// }  // namespace OpenGL

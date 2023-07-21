#pragma once
#include <glad/gl.h>

#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "magic_enum.hpp"
#include "support/utils.hpp"

// OpenGL Helper stuff for psx related graphics

namespace OpenGL {

	// https://stackoverflow.com/questions/53945490/how-to-assert-that-a-constexpr-if-else-clause-never-happen
	template <class...>
	constexpr std::false_type AlwaysFalse{};

	enum FB {
		Default = GL_FRAMEBUFFER,
		Read = GL_READ_FRAMEBUFFER,
		Draw = GL_DRAW_FRAMEBUFFER,
	};

	enum Filtering {
		Linear = GL_LINEAR,
		Nearest = GL_NEAREST,
	};

	enum VB {
		ArrayBuffer = GL_ARRAY_BUFFER,
		IndexBuffer = GL_ELEMENT_ARRAY_BUFFER,

	};

	enum BufferUsage {
		StreamDraw = GL_STREAM_DRAW,
		DynamicDraw = GL_DYNAMIC_DRAW,
		StaticDraw = GL_STATIC_DRAW,
	};

	enum DrawMode {
		Trianges = GL_TRIANGLES,
		TriangleStrip = GL_TRIANGLE_STRIP,
		Quad = GL_QUADS,
	};

	template <DrawMode mode>
	void drawArrays(GLsizei first, GLsizei count) {
		glDrawArrays(static_cast<GLenum>(mode), first, count);
	}

	static inline void bindDefaultFramebuffer() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
	static inline void bindDefaultTexture() { glBindTexture(GL_TEXTURE_2D, 0); }
	static inline void setclearColor(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f) { glClearColor(r, g, b, a); }
	static inline void clearColor() { glClear(GL_COLOR_BUFFER_BIT); }
	static inline void setPackAlignment(GLint param) { glPixelStorei(GL_PACK_ALIGNMENT, param); }
	static inline void setUnpackAlignment(GLint param) { glPixelStorei(GL_UNPACK_ALIGNMENT, param); }

	static inline void setScissor(GLsizei width, GLsizei height) { glScissor(0, 0, width, height); }
	static inline void setScissor(GLsizei x, GLsizei y, GLsizei width, GLsizei height) { glScissor(x, y, width, height); }
	static inline void enableScissor() { glEnable(GL_SCISSOR_TEST); }
	static inline void disableScissor() { glDisable(GL_SCISSOR_TEST); }
	static inline void enableBlend() { glEnable(GL_BLEND); }
	static inline void disableBlend() { glDisable(GL_BLEND); }

	template <VB type>
	class VertexBuffer {
	  public:
		VertexBuffer() {}
		~VertexBuffer() {
			if (m_handle) glDeleteBuffers(1, &m_handle);
		}

		template <BufferUsage draw>
		void gen(GLsizei size) {
			glGenBuffers(1, &m_handle);
			bind();
			glBufferData(static_cast<GLenum>(type), size, nullptr, static_cast<GLenum>(draw));
			m_size = size;
		}

		void gen() {
			glGenBuffers(1, &m_handle);
			bind();
		}

		GLuint get() { return m_handle; }
		size_t size() { return m_size; }
		void bind() { glBindBuffer(static_cast<GLenum>(type), m_handle); }
		void unbind() { bindDefaultFramebuffer(); }

		template <typename T, BufferUsage usage>
		void bufferData(T* vertices, size_t count) {
			glBufferData(static_cast<GLenum>(type), sizeof(T) * count, vertices, static_cast<GLenum>(usage));
		}

		template <typename T>
		void bufferSubData(T* vertices, size_t count, GLintptr offset = 0) {
			glBufferSubData(static_cast<GLenum>(type), offset, sizeof(T) * count, vertices);
		}

	  private:
		GLuint m_handle;
		GLsizei m_size;
	};

	class VertexArray {
	  public:
		VertexArray() {}
		~VertexArray() {
			if (m_handle) glDeleteVertexArrays(1, &m_handle);
		}
		void gen() { glGenVertexArrays(1, &m_handle); }
		GLuint get() { return m_handle; }
		void bind() { glBindVertexArray(m_handle); }
		void unbind() { glBindVertexArray(0); }

		template <GLuint index>
		void setAttributeI(GLint size, GLenum type, GLsizei stride, const void* pointer) {
			glVertexAttribIPointer(index, size, type, stride, pointer);
		}

		template <GLuint index>
		void setAttribute(GLint size, GLenum type, bool normalized, GLsizei stride, const void* pointer) {
			glVertexAttribPointer(index, size, type, normalized, stride, pointer);
		}

		// glVertexAttribPointer casts to float

		template <typename T, GLuint index>
		void setAttribute(GLint size, GLsizei stride, const void* pointer, bool normalized = false) {
			if constexpr (std::is_same_v<T, GLfloat>) {
				glVertexAttribPointer(index, size, GL_FLOAT, normalized, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLubyte>) {
				glVertexAttribPointer(index, size, GL_UNSIGNED_BYTE, normalized, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLbyte>) {
				glVertexAttribPointer(index, size, GL_BYTE, normalized, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLushort>) {
				glVertexAttribPointer(index, size, GL_UNSIGNED_SHORT, normalized, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLshort>) {
				glVertexAttribPointer(index, size, GL_SHORT, normalized, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLuint>) {
				glVertexAttribPointer(index, size, GL_UNSIGNED_INT, normalized, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLint>) {
				glVertexAttribPointer(index, size, GL_INT, normalized, stride, pointer);
			} else {
				static_assert(AlwaysFalse<T>, "Unsupported type for VertexArray::setAttribute<>()");
			}
		}

		// glVertexAttribPointer does not cast to float
		template <typename T, GLuint index>
		void setAttributeI(GLint size, GLsizei stride, const void* pointer) {
			if constexpr (std::is_same_v<T, GLfloat>) {
				glVertexAttribIPointer(index, size, GL_FLOAT, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLubyte>) {
				glVertexAttribIPointer(index, size, GL_UNSIGNED_BYTE, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLbyte>) {
				glVertexAttribIPointer(index, size, GL_BYTE, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLushort>) {
				glVertexAttribIPointer(index, size, GL_UNSIGNED_SHORT, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLshort>) {
				glVertexAttribIPointer(index, size, GL_SHORT, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLuint>) {
				glVertexAttribIPointer(index, size, GL_UNSIGNED_INT, stride, pointer);
			} else if constexpr (std::is_same_v<T, GLint>) {
				glVertexAttribIPointer(index, size, GL_INT, stride, pointer);
			} else {
				static_assert(AlwaysFalse<T>, "Unsupported type for VertexArray::setAttribute<>()");
			}
		}

		template <GLuint index>
		void enableAttribute() {
			glEnableVertexAttribArray(index);
		}

	  private:
		GLuint m_handle;
	};

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

		void gen() { glGenFramebuffers(1, &m_handle); }
		void attachTexture(GLuint texture) { glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0); }

		template <FB type = Default>
		void bind() {
			glBindFramebuffer(static_cast<GLenum>(type), m_handle);
		}
		void unbind() { bindDefaultFramebuffer(); }
		GLuint get() { return m_handle; }

	  private:
		GLuint m_handle;
	};

	// Maybe fix later to support different bindings?
	class Texture {
	  public:
		Texture() {}
		~Texture() {
			if (m_handle) glDeleteTextures(1, &m_handle);
		}

		void gen(GLenum internalFormat, GLsizei width, GLsizei height) {
			glGenTextures(1, &m_handle);
			bind();
			glTexStorage2D(GL_TEXTURE_2D, 1, internalFormat, width, height);
		}

		void bind() { glBindTexture(GL_TEXTURE_2D, m_handle); }
		void unbind() { glBindTexture(GL_TEXTURE_2D, 0); }
		GLuint get() { return m_handle; }

		template <Filtering filter>
		void setFiltering() {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLenum>(filter));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLenum>(filter));
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}

	  private:
		GLuint m_handle;
	};

	enum class UniformType { UInt, Int, Float, Vec2, Vec3, Vec4 };

	class Shader {
		using fspath = std::filesystem::path;

	  public:
		Shader() {}

		~Shader() {
			if (m_program) glDeleteProgram(m_program);
		}

		void loadShadersFromString(const char* vertexShaderSource, const char* fragmentShaderSource) {
			m_vertexShaderSource = vertexShaderSource;
			m_fragmentShaderSource = fragmentShaderSource;
			m_fromFile = false;
			build();
		}

		void loadShadersFromFile(const fspath vertexShaderPath, const fspath fragmentShaderPath) {
			m_vertexShaderPath = vertexShaderPath;
			m_fragmentShaderPath = fragmentShaderPath;
			m_fromFile = true;
			std::ifstream vertexFile, fragmentFile;
			vertexFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
			fragmentFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);

			try {
				vertexFile.open(m_vertexShaderPath);
				std::stringstream vss;
				vss << vertexFile.rdbuf();
				vertexFile.close();
				m_vertexShaderSource = vss.str();
			} catch (std::ifstream::failure& error) {
				Helpers::panic("Failed to load Vertex Shader: {} ", error.what());
			}

			try {
				fragmentFile.open(m_fragmentShaderPath);
				std::stringstream fss;
				fss << fragmentFile.rdbuf();
				fragmentFile.close();
				m_fragmentShaderSource = fss.str();
			} catch (std::ifstream::failure& error) {
				Helpers::panic("Failed to load Fragment Shader: {}", error.what());
			}

			build();
		}

		void build() {
			const char* vertexShaderSourceFinal = m_vertexShaderSource.c_str();
			const char* fragmentShaderSourceFinal = m_fragmentShaderSource.c_str();

			unsigned int vertexShader, fragmentShader;

			vertexShader = glCreateShader(GL_VERTEX_SHADER);
			glShaderSource(vertexShader, 1, &vertexShaderSourceFinal, NULL);
			glCompileShader(vertexShader);
			verifyShaderCompile(vertexShader, ShaderType::Vertex);

			fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(fragmentShader, 1, &fragmentShaderSourceFinal, NULL);
			glCompileShader(fragmentShader);
			verifyShaderCompile(fragmentShader, ShaderType::Fragment);

			m_program = glCreateProgram();
			glAttachShader(m_program, vertexShader);
			glAttachShader(m_program, fragmentShader);
			glLinkProgram(m_program);
			verifyShaderCompile(m_program, ShaderType::Program);

			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);
		}

		void activate() { glUseProgram(m_program); }

		void deactivate() { glUseProgram(0); }

		enum class ShaderType { Vertex, Fragment, Program };

		void verifyShaderCompile(GLuint shader, ShaderType type) {
			GLint result;
			GLint length;
			std::string log;
			if (type == ShaderType::Program) {
				glGetProgramiv(shader, GL_LINK_STATUS, &result);
				if (result != GL_TRUE) {
					glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &length);
					log.resize(length);
					glGetProgramInfoLog(shader, length, &length, log.data());
					Helpers::panic("Error Linking Shader Program: {}\n", log);
				}
			} else {
				glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
				if (result != GL_TRUE) {
					glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
					log.resize(length);
					Helpers::panic("Error Compiling {} shader: {}\n", magic_enum::enum_name(type), log.data());
				}
			}
		}

		void recompile() {
			glUseProgram(0);
			glDeleteProgram(m_program);
			if (m_fromFile) loadShadersFromFile(m_vertexShaderPath, m_fragmentShaderPath);
			build();
		};

		GLuint ID() { return m_program; }

	  private:
		fspath m_vertexShaderPath;
		fspath m_fragmentShaderPath;
		std::string m_vertexShaderSource;
		std::string m_fragmentShaderSource;
		GLuint m_program;
		bool m_fromFile = false;
		friend class ShaderUniform;
	};

	class ShaderUniform {
		ShaderUniform(std::string&& name, UniformType type) : m_name(std::move(name)), m_type(type) {}

		void setLocation(Shader& shader) { m_location = glGetUniformLocation(shader.m_program, m_name.c_str()); }

		template <typename T>
		void set(T value) {
			switch (m_type) {
				case UniformType::UInt:
					static_assert(std::is_same_v<T, GLuint>, "Uniform type mismatch");
					glUniform1ui(m_location, value);
					break;
				case UniformType::Int:
					static_assert(std::is_same_v<T, GLint>, "Uniform type mismatch");
					glUniform1i(m_location, value);
					break;
				case UniformType::Float:
					static_assert(std::is_same_v<T, GLfloat>, "Uniform type mismatch");
					glUniform1f(m_location, value);
					break;
			}
		}

		template <typename T>
		void set(T value1, T value2) {
			switch (m_type) {
				case UniformType::UInt:
					static_assert(std::is_same_v<T, GLuint>, "Uniform type mismatch");
					glUniform2ui(m_location, value1, value2);
					break;
				case UniformType::Int:
					static_assert(std::is_same_v<T, GLint>, "Uniform type mismatch");
					glUniform2i(m_location, value1, value2);
					break;
				case UniformType::Float:
					static_assert(std::is_same_v<T, GLfloat>, "Uniform type mismatch");
					glUniform2f(m_location, value1, value2);
					break;
			}
		}

		template <typename T>
		void set(T value1, T value2, T value3) {
			switch (m_type) {
				case UniformType::UInt:
					static_assert(std::is_same_v<T, GLuint>, "Uniform type mismatch");
					glUniform3ui(m_location, value1, value2, value3);
					break;
				case UniformType::Int:
					static_assert(std::is_same_v<T, GLint>, "Uniform type mismatch");
					glUniform3i(m_location, value1, value2, value3);
					break;
				case UniformType::Float:
					static_assert(std::is_same_v<T, GLfloat>, "Uniform type mismatch");
					glUniform3f(m_location, value1, value2, value3);
					break;
			}
		}

	  private:
		GLint m_location;
		std::string m_name;
		UniformType m_type;
	};

	template <typename T>
	struct Rect {
		T x;
		T y;
		T w;
		T h;

		Rect(T x, T y, T w, T h) : x(x), y(y), w(w), h(h) {}
		Rect() : Rect(0, 0, 0, 0) {}
	};

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

}  // namespace OpenGL

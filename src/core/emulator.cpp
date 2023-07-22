#include "emulator.hpp"

#include <vector>

#include "glad/gl.h"

#define OPENGL_SHADER_VERSION "#version 410\n"

Emulator::Emulator() : bus(cpu), cpu(bus), scheduler(cpu.cycleRef()) {
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
				Helpers::panic("Error initializing SDL: {}", SDL_GetError());
		}

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);

		auto window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
		window = SDL_CreateWindow("Test", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, window_flags);

		if (window == nullptr) {
				Helpers::panic("Error creating SDL Window: {}", SDL_GetError());
		}

		glContext = SDL_GL_CreateContext(window);
		if (glContext == nullptr) {
				Helpers::panic("Error creating SDL Context: {}", SDL_GetError());
		}

		SDL_GL_MakeCurrent(window, glContext);
		SDL_GL_SetSwapInterval(1);  // VSync on by default

		if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress)) {
				Helpers::panic("Error initializing glad GL Loader: {}", SDL_GetError());
		}

		setupShaders();
		setupDisplay();
		scheduleVblank();
}

Emulator::~Emulator() {
		SDL_GL_DeleteContext(glContext);
		SDL_DestroyWindow(window);
		SDL_Quit();
}

void Emulator::run() {}

void Emulator::runFrame() {
		while (cpu.m_cycles < scheduler.nextEventCycles()) {
				cpu.step();
		}
		scheduler.handleEvents();
}

void Emulator::stop() { running = false; }

void Emulator::start() { running = true; }

void Emulator::reset() {
		running = false;
		vblank = false;
		frameCounter = 0;
}

void Emulator::update() {
		auto startTime = SDL_GetTicks();
		SDL_Event event;

		if (SDL_PollEvent(&event)) {
				if (event.type == SDL_QUIT) open = false;
				if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
						open = false;
				}
		}

		while (running && !vblank) {
				runFrame();
		}

		vblank = false;
		scheduleVblank();

		drawDisplay();

		SDL_GL_SwapWindow(window);

		auto endTime = SDL_GetTicks() - startTime;
		auto currentFPS = (endTime > 0) ? 1000.0f / endTime : 0.0f;
		frameCounter++;
		SDL_SetWindowTitle(window, fmt::format(fmt::runtime("ShitStation - {:.2f} FPS/ {:.3f}ms"), currentFPS, float(endTime)).c_str());
}

void Emulator::setupShaders() {
		// clang-format off
	static const char* vertexSource = OPENGL_SHADER_VERSION R"(
		layout (location = 0) in vec2 aPos;
		layout (location = 1) in vec2 aTexCoords;
		out vec2 TexCoords;

		void main() {
			gl_Position = vec4(aPos.x, aPos.y, 0.0, 1.0);
			TexCoords = aTexCoords;
		}
	)";

	static const char* fragSource = OPENGL_SHADER_VERSION R"(
		out vec4 FragColor;
		in vec2 TexCoords;
		uniform sampler2D screenTexture;

		void main() {
			//FragColor = texture(screenTexture, TexCoords);
			FragColor = vec4(0.0, 1.0, 0.0, 1.0);
		}
	)";

		// clang-format on

		screenShader.loadShadersFromString(vertexSource, fragSource);
}

void Emulator::setupDisplay() {
		std::vector<ScreenVertex> vertices = {
				{OpenGL::vec2({-1.0f, -1.0f}), OpenGL::vec2({0, 0})},  // top left
				{OpenGL::vec2({1.0f, -1.0f}), OpenGL::vec2({1, 0})},   // bottom left
				{OpenGL::vec2({-1.0f, 1.0f}), OpenGL::vec2({0, 1})},   // bottom right
				{OpenGL::vec2({1.0f, 1.0f}), OpenGL::vec2({1, 1})}     // top right
		};

		screenVAO.gen();
		screenVAO.bind();

		screenVBO.gen();
		screenVBO.bind();
		screenVBO.bufferData<ScreenVertex, OpenGL::StaticDraw>(vertices.data(), vertices.size());
		screenVAO.setAttribute<0>(2, GL_FLOAT, GL_FALSE, sizeof(ScreenVertex), reinterpret_cast<void*>(offsetof(ScreenVertex, pos)));
		screenVAO.enableAttribute<0>();
		screenVAO.setAttribute<1>(2, GL_FLOAT, GL_FALSE, sizeof(ScreenVertex), reinterpret_cast<void*>(offsetof(ScreenVertex, uv)));
}

void Emulator::drawDisplay() {
		screenVAO.bind();
		screenVBO.bind();
		screenShader.activate();
		OpenGL::setclearColor();
		OpenGL::clearColor();
		OpenGL::drawArrays<OpenGL::TriangleStrip>(0, 4);
}

void Emulator::scheduleVblank() {
		scheduler.scheduleEvent(cyclesPerFrame, [&]{
				bus.triggerInterrupt(InterruptType::VBlank);
				vblank = true;
		});
}

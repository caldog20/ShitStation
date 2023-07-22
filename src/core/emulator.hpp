#pragma once
#include <SDL.h>

#include "core/bus.hpp"
#include "core/cpu.hpp"
#include "core/cdrom.hpp"
#include "core/scheduler.hpp"
#include "support/opengl.hpp"
#include "support/utils.hpp"


class Emulator {
	public:
		Emulator();
		~Emulator();

		void run();
		void runFrame();
		void stop();
		void start();
		void reset();
		void update();

		[[nodiscard]] bool isOpen() const { return open; }
		[[nodiscard]] bool isRunning() const { return running; }

		static constexpr int cpuFrequency = 33868800;
		static constexpr int fps = 60;
		static constexpr int cyclesPerFrame = cpuFrequency / fps;
		static constexpr int width = 1280;
		static constexpr int height = 720;

		Bus bus;
		Cpu cpu;
		Scheduler scheduler;

	private:
		void scheduleVblank();
		void setupShaders();
		void setupDisplay();
		void drawDisplay();

		SDL_Window* window = nullptr;
		SDL_GLContext glContext;
		bool running = true;
		bool open = true;
		bool vblank = false;
		u64 frameCounter = 0;

		// Display Stuff
		OpenGL::Shader screenShader;
		OpenGL::VertexArray screenVAO;
		OpenGL::VertexBuffer<OpenGL::ArrayBuffer> screenVBO;

		struct ScreenVertex {
				OpenGL::vec2 pos;
				OpenGL::vec2 uv;
		};
};

#include "psx.hpp"

#include <fstream>

#include "glad/gl.h"

#define OPENGL_SHADER_VERSION "#version 410\n"

PSX::PSX()
    : bus(cpu, dma, timers, cdrom, sio, gpu), cpu(bus), scheduler(bus, cpu), dma(bus, scheduler), timers(scheduler), cdrom(scheduler),
      sio(scheduler) {
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
			FragColor = texture(screenTexture, TexCoords);
			//FragColor = vec4(1.0, 1.0, 0.0, 1.0);
		}
	)";

    // clang-format on

    screenShader.loadShadersFromString(vertexSource, fragSource);

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

    gpu.init(); // Init GPU after OpenGL is initialized
    reset();
}

PSX::~PSX() {
    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void PSX::reset() {
    frameCounter = 0;
    running = false;
    cpu.reset();
    bus.reset();
    scheduler.reset();
    dma.reset();
    timers.reset();
    gpu.reset();
    cdrom.reset();
    tempScheduleVBlank();  // schedule first vblank until gpu is implemented
}

void PSX::runFrame() {
    // Run until we hit vblank
    while (!vblank) {
        auto& cycleTarget = cpu.getCycleTargetRef();
        auto& totalCycles = cpu.getCycleRef();
        cycleTarget = scheduler.nextEventCycles();
        // cycleTarget can be dynamically updated by the scheduler when new events are added
        // Figure out a cleaner way to handle this
        while (totalCycles < cycleTarget) {
            cpu.step();
        }
        scheduler.handleEvents();
    }
}

void PSX::start() {
    // Check for loaded BIOS here
    if (!biosLoaded) {
        Log::warn("No BIOS loaded\n");
        return;
    }

    running = true;
}

void PSX::stop() { running = false; }

void PSX::tempScheduleVBlank() {
    scheduler.scheduleEvent(cyclesPerFrame, [&]() {
        bus.triggerInterrupt(Bus::IRQ::VBLANK);
        vblank = true;
        //        Log::debug("VBLANK at {} cycles\n", cpu.getTotalCycles());
    });
}

void PSX::update() {
    auto startTime = SDL_GetTicks();
    SDL_PollEvent(&event);
    if (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) open = false;
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window)) {
            open = false;
        }
    }

    if (running) {
        runFrame();
    }

    tempScheduleVBlank();
    vblank = false;

    screenVAO.bind();
    screenVBO.bind();
    screenShader.activate();
    OpenGL::setclearColor();
    OpenGL::clearColor();
    OpenGL::drawArrays<OpenGL::TriangleStrip>(0, 4);

    SDL_GL_SwapWindow(window);

    auto endTime = SDL_GetTicks() - startTime;
    auto currentFPS = (endTime > 0) ? 1000.0f / endTime : 0.0f;
    frameCounter++;
    SDL_SetWindowTitle(window, fmt::format(fmt::runtime("ShitStation - {:.2f} FPS/ {:.3f}ms"), currentFPS, float(endTime)).c_str());
}

void PSX::loadBIOS(const std::filesystem::path& path) {
    biosLoaded = false;
    if (!std::filesystem::exists(path)) {
        Log::warn("File at {} does not exist\n", path.string());
        return;
    }

    auto file = std::ifstream(path, std::ios::binary);
    if (file.fail()) {
        Log::warn("Cannot open file at {}\n", path.string());
        return;
    }

    file.unsetf(std::ios::skipws);
    auto fileSize = std::filesystem::file_size(path);
    auto buffer = std::vector<u8>(fileSize);

    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(fileSize));

    file.close();

    std::memcpy(bus.getBiosPointer(), buffer.data(), buffer.size());
    biosLoaded = true;
}

void PSX::loadDisc(const std::filesystem::path& path) {}

void PSX::sideload(const std::filesystem::path& path) {
    u32 initialPC = 0;
    u32 size = 0;
    u32 gp = 0;
    u32 address = 0;
    std::vector<u8> exe;

    if (!std::filesystem::exists(path)) {
        Log::warn("[Sideload] File at {} does not exist\n", path.string());
        return;
    }

    auto file = std::ifstream(path, std::ios::binary);
    if (file.fail()) {
        Log::warn("[Sideload] Cannot open file at {}\n", path.string());
        return;
    }

    file.unsetf(std::ios::skipws);

    file.seekg(std::ios::beg);
    std::string magic(8, '\0');
    file.read(magic.data(), 8);
    if (magic != "PS-X EXE") {
        Log::warn("[Sideload] {} is not a valid PS-EXE\n", path.filename().string());
        return;
    }

    file.seekg(16, std::ios::beg);
    file.read(reinterpret_cast<char*>(&initialPC), 4);
    file.read(reinterpret_cast<char*>(&gp), 4);
    file.read(reinterpret_cast<char*>(&address), 4);
    file.read(reinterpret_cast<char*>(&size), 4);

    exe.resize(size);
    file.seekg(0x800, std::ios::beg);
    file.read(reinterpret_cast<char*>(exe.data()), static_cast<std::streamsize>(size));
    file.close();

    bus.setSideload(address, initialPC, exe);
}

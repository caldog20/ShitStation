#pragma once
#include <SDL.h>

#include <filesystem>

#include "bus/bus.hpp"
#include "cdrom/cdrom.hpp"
#include "cpu/cpu.hpp"
#include "dma/dmacontroller.hpp"
#include "gpu/gpu.hpp"
#include "gpu/gpugl.hpp"
#include "gpu/softgpu.hpp"
#include "scheduler/scheduler.hpp"
#include "sio/sio.hpp"
#include "spu/spu.hpp"
#include "support/helpers.hpp"
#include "support/log.hpp"
#include "support/opengl.hpp"
#include "timers/timers.hpp"

class PSX {
  public:
    PSX();
    ~PSX();

    void reset();
    void runFrame();

    void start();
    void stop();

    void update();

    [[nodiscard]] bool isOpen() const { return open; }

    void loadBIOS(const std::filesystem::path& path);
    void loadDisc(const std::filesystem::path& path);
    void sideload(const std::filesystem::path& path);

    static constexpr u32 clockrate = 33868800;
    static constexpr u32 framerate = 60;
    static constexpr u32 width = 1280;
    static constexpr u32 height = 720;
    static constexpr u32 cyclesPerFrame = clockrate / framerate;

  private:
    Bus::Bus bus;
    Cpu::Cpu cpu;
    Scheduler::Scheduler scheduler;
    DMA::DMA dma;
    Timers::Timers timers;
    GPU::GPU_GL gpu;

    CDROM::CDROM cdrom;
    SIO::SIO sio;
    Spu::Spu spu;

    SDL_Renderer* renderer;
    SDL_Window* window;
    SDL_Texture* texture;
    SDL_GLContext glContext;
    SDL_Event event;

    bool running = false;
    bool vblank = false;
    bool biosLoaded = false;
    bool open = true;
    void tempScheduleVBlank();
    u64 frameCounter = 0;
    OpenGL::ShaderProgram screenShader;
    OpenGL::VertexArray screenVAO;
    OpenGL::VertexBuffer screenVBO;
    GLint uniformTextureLocation = 0;
};

#pragma once
#include <SDL.h>

#include <filesystem>

#include "bus/bus.hpp"
#include "cdrom/cdrom.hpp"
#include "cpu/cpu.hpp"
#include "dma/dmacontroller.hpp"
#include "gpu/gpu.hpp"
#include "scheduler/scheduler.hpp"
#include "sio/sio.hpp"
#include "support/helpers.hpp"
#include "support/log.hpp"
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
    //    GPU::GPU gpu;
    CDROM::CDROM cdrom;
    SIO::SIO sio;

    SDL_Renderer* renderer;
    SDL_Window* window;
    SDL_Texture* texture;

    bool running = false;
    bool vblank = false;
    bool biosLoaded = false;

    void tempScheduleVBlank();
};

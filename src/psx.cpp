#include "psx.hpp"

#include <fstream>

PSX::PSX()
    : bus(cpu, dma, timers, cdrom, sio), cpu(bus), scheduler(bus, cpu), dma(bus, scheduler), timers(scheduler), cdrom(scheduler), sio(scheduler) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");

    SDL_CreateWindowAndRenderer(1024, 512, 0, &window, &renderer);
    SDL_SetWindowSize(window, 1024, 512);
    SDL_RenderSetLogicalSize(renderer, 1024, 512);
    SDL_SetWindowResizable(window, SDL_FALSE);
    SDL_SetWindowTitle(window, "Emulator");

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_XBGR1555, SDL_TEXTUREACCESS_STREAMING, 1024, 512);

    reset();
}

PSX::~PSX() {}

void PSX::reset() {
    running = false;
    cpu.reset();
    bus.reset();
    scheduler.reset();
    dma.reset();
    timers.reset();
    //    gpu.reset();
    GPU::init();
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
    SDL_Event e;
    SDL_PollEvent(&e);

    if (running) {
        runFrame();
    }

    SDL_UpdateTexture(texture, nullptr, (u8*)GPU::getVRAM().data(), 2 * 1024);
    SDL_RenderCopy(renderer, texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);

    tempScheduleVBlank();
    vblank = false;
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

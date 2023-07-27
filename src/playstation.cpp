#include "playstation.hpp"

#include <fstream>

Playstation::Playstation() : cpu(bus), bus(cpu), scheduler(bus, cpu) { reset(); }

Playstation::~Playstation() {}

void Playstation::reset() {
    running = false;
    cpu.reset();
    bus.reset();
    scheduler.reset();
    tempScheduleVBlank();  // schedule first vblank until gpu is implemented
}

void Playstation::runFrame() {
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

void Playstation::start() {
    // Check for loaded BIOS here
    if (!biosLoaded) {
        Log::warn("No BIOS loaded\n");
        return;
    }

    running = true;
}

void Playstation::stop() { running = false; }

void Playstation::tempScheduleVBlank() {
    scheduler.scheduleEvent(cyclesPerFrame, [&]() {
        bus.triggerInterrupt(Bus::IRQ::VBLANK);
        vblank = true;
        Log::debug("VBLANK at {} cycles\n", cpu.getTotalCycles());
    });
}

void Playstation::update() {
    if (running) {
        runFrame();
    }

    tempScheduleVBlank();
    vblank = false;
}

void Playstation::loadBIOS(const std::filesystem::path& path) {
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

void Playstation::loadDisc(const std::filesystem::path& path) {}

void Playstation::sideload(const std::filesystem::path& path) {
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

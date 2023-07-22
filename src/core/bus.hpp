#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
#include <string>
#include <fstream>
#include "support/utils.hpp"
#include "BitField.hpp"

enum class InterruptType : u16 {
		VBlank = 0,
		GPU = 1,
		CDROM = 2,
		Dma = 3,
		Timer0 = 4,
		Timer1 = 5,
		Timer2 = 6,
		Sio0 = 7,
		Sio1 = 8,
		Spu = 9,
		Lp = 10
};

enum {
		BIAS = 2,
		MEM_ACCESS_BIAS = 1,
		BIOS_ACCESS_BIAS = 21,
};

struct CacheControl {
		u32 r;
		BitField<0, 3, u32> unknown1;
		BitField<3, 1, u32> scratchpadEnable1;
		BitField<4, 2, u32> unknown2;
		BitField<6, 1, u32> unknown3;
		BitField<7, 1, u32> scratchpadEnable2;
		BitField<8, 1, u32> unknown4;
		BitField<9, 1, u32> crashMode;
		BitField<10, 1, u32> unknown5;
		BitField<11, 1, u32> cacheEnable;
		BitField<12, 20, u32> unknown6;
};

struct RamSize {
		u32 r;
		BitField<0, 3, u32> unknown1;
		BitField<3, 1, u32> crashZero;
		BitField<4, 3, u32> unknown2;
		BitField<7, 1, u32> delay;
		BitField<8, 1, u32> unknown3;
		BitField<9, 3, u32> window;
		BitField<12, 4, u32> unknown4;
		BitField<16, 16, u32> unknown5;
};

// IRQ
struct InterruptControl {
		union {
				u32 r;
				BitField<0, 1, u32> VBLANK;
				BitField<1, 1, u32> GPU;
				BitField<2, 1, u32> CDROM;
				BitField<3, 1, u32> DMA;
				BitField<4, 1, u32> TIMER0;
				BitField<5, 1, u32> TIMER1;
				BitField<6, 1, u32> TIMER2;
				BitField<7, 1, u32> SIO0;
				BitField<8, 1, u32> SIO1;
				BitField<9, 1, u32> SPU;
				BitField<10, 1, u32> LP;
				BitField<11, 21, u32> unused;
		} ISTAT;

		union {
				u32 r;
				BitField<0, 1, u32> VBLANK;
				BitField<1, 1, u32> GPU;
				BitField<2, 1, u32> CDROM;
				BitField<3, 1, u32> DMA;
				BitField<4, 1, u32> TIMER0;
				BitField<5, 1, u32> TIMER1;
				BitField<6, 1, u32> TIMER2;
				BitField<7, 1, u32> SIO0;
				BitField<8, 1, u32> SIO1;
				BitField<9, 1, u32> SPU;
				BitField<10, 1, u32> LP;
				BitField<11, 21, u32> unused;
		} IMASK;
};

struct Sideload {
		u32 initialPC = 0;
		u32 size = 0;
		u32 gp = 0;
		u32 address = 0;
		bool ready = false;
		std::vector<u8> binary;

		void load(const std::filesystem::path& path) {
				ready = false;
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

				file.seekg(std::ios::beg);
				std::string magic(8, '\0');
				file.read(magic.data(), 8);
				if (magic != "PS-X EXE") {
						Log::warn("{} is not a valid PS-EXE\n", path.filename().string());
						return;
				}

				file.seekg(16, std::ios::beg);
				file.read(reinterpret_cast<char*>(&initialPC), 4);
				file.read(reinterpret_cast<char*>(&gp), 4);
				file.read(reinterpret_cast<char*>(&address), 4);
				file.read(reinterpret_cast<char*>(&size), 4);

				binary.resize(size);
				file.seekg(0x800, std::ios::beg);
				file.read(reinterpret_cast<char*>(binary.data()), static_cast<std::streamsize>(size));
				file.close();
				ready = true;
		}
};

class Cpu;

class Bus {
	public:
		explicit Bus(Cpu& cpu);
		~Bus();
		void init();
		void reset();

		// Bios Related Methods
		void loadBIOS(const std::filesystem::path& path);
		void patchBIOS();
		void sideload();
		void prepSideload(const std::filesystem::path& path);
		void shellReached();
		u32 fetchInstruction(u32 address);

		[[nodiscard]] bool isBiosLoaded() const { return biosLoaded; }

		[[nodiscard]] bool irqActive() const { return (m_interruptControl.ISTAT.r & m_interruptControl.IMASK.r) != 0; }

		template <typename T>
		T read(u32 address);

		template <typename T>
		void write(u32 address, T value);

		// Move to emulator or cpu
		void triggerInterrupt(InterruptType interrupt);

		bool biosLoaded = false;
		bool sideloading = false;
		Sideload m_sideloadInfo;

		u8* m_ram = nullptr;
		u8* m_bios = nullptr;
		u8* m_scratchpad = nullptr;
		u8* m_spuram = nullptr;
		u8* m_timers = nullptr;

		uintptr_t* m_readPages = nullptr;
		uintptr_t* m_writePages = nullptr;

		CacheControl m_cacheControl;
		InterruptControl m_interruptControl;
		RamSize m_ramSize;
		u8 m_memControl[36];
		u32 m_memControl2;

	private:
		Cpu& cpu;

	public:
		enum : u32 {
				BIOS_SIZE = 512 * 1024,
				RAM_SIZE = 2 * 1024 * 1024,
				SCRATCHPAD_SIZE = 1 * 1024,
				ICACHE_SIZE = 4 * 1024,
				EXP1_SIZE = 8 * 1024 * 1024,
				EXP2_SIZE = 128,
				SPU_SIZE = 640,
				TIMERS_SIZE = 48,
		};

		const Range<u32> TIMERS = Range<u32>(0x1f801100, TIMERS_SIZE);
		const Range<u32> RAMSIZE = Range<u32>(0x1f801060, 4);
		const Range<u32> SPU = Range<u32>(0x1f801c00, 640);
		const Range<u32> EXPANSION_1 = Range<u32>(0x1f000000, 0x800000);
		const Range<u32> EXPANSION_2 = Range<u32>(0x1f802000, 0x80);
		const Range<u32> CACHE_CONTROL = Range<u32>(0xfffe0130, 4);
		const Range<u32> MEM_CONTROL = Range<u32>(0x1f801000, 36);
		const Range<u32> CDROM = Range<u32>(0x1f801800, 0x4);
		const Range<u32> PAD_MEMCARD = Range<u32>(0x1f801040, 15);
		const Range<u32> DMA = Range<u32>(0x1f801080, 0x7C);
		const Range<u32> GPU = Range<u32>(0x1f801810, 8);
		const Range<u32> INTERRUPT_CONTROL = Range<u32>(0x1f801070, 8);
		const Range<u32> MEM_CONTROL2 = Range<u32>(0x1F801060, 4);

		const u32 region_mask[8] = {
				0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff, 0x1fffffff, 0xffffffff, 0xffffffff,
		};

		inline u32 mask(u32 address) { return address & region_mask[address >> 29]; }
};

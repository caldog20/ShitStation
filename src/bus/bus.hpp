#pragma once
#include <cassert>

#include "support/helpers.hpp"

namespace Bus {

	enum IRQ : u16 { VBLANK = 0, GPU, CDROM, DMA, TIMER0, TIMER1, TIMER2, PAD, SIO, SPU, LIGHTPEN };

	struct Range {
		Range(u32 base, u32 size) : base(base), size(size) {}

		inline bool contains(u32 addr) const { return (addr >= base && addr < base + size); }
		inline u32 offset(u32 addr) const { return addr - base; }

		u32 base;
		u32 size;
	};

	namespace MemorySize {
		enum : u32 { Bios = 512_KB, Ram = 2_MB, Scratchpad = 1_KB, FastMem = 64_KB };
	}

	namespace CycleBias {
		enum : u32 {
			ROM = 21,
			RAM = 1,
			CPU = 2,
		};
	}

	class Bus {
	  public:
		Bus();
		~Bus();

		void reset();

		u8 read8(u32 address);
		u16 read16(u32 address);
		u32 read32(u32 address);

		void write8(u32 address, u8 value);
		void write16(u32 address, u16 value);
		void write32(u32 address, u32 value);

		[[nodiscard]] bool isIRQPending() const { return (ISTAT & IMASK) != 0; }

		template <typename T>
		T read(u32 address) {
			if constexpr (std::is_same_v<T, u32>)
				return read32(address);
			else if constexpr (std::is_same_v<T, u16>)
				return read16(address);
			else if constexpr (std::is_same_v<T, u8>)
				return read8(address);
		}

		template <typename T>
		void write(u32 address, T value) {
			if constexpr (std::is_same_v<T, u32>)
				write32(address, value);
			else if constexpr (std::is_same_v<T, u16>)
				write16(address, value);
			else if constexpr (std::is_same_v<T, u8>)
				write8(address, value);
		}

		template <typename T = u8>
		[[nodiscard]] auto getRamPointer(u32 address = 0) -> T* {
			auto addr = mask(address);
			assert(RAM.contains(addr));
			return (T*)&ram[RAM.offset(addr)];
		}

		template <typename T = u8>
		[[nodiscard]] auto getBiosPointer(u32 address = 0) -> T* {
			auto addr = mask(address);
			assert(BIOS.contains(addr));
			return (T*)&bios[BIOS.offset(addr)];
		}

	  private:
		u32 CacheControl;
		u32 MemControl[36];
		u32 MemControl2;
		u16 ISTAT;
		u16 IMASK;

		u8* ram = nullptr;
		u8* bios = nullptr;
		u8* scratchpad = nullptr;

		uintptr_t* readPages = nullptr;
		uintptr_t* writePages = nullptr;

		const Range RAM = {0x00000000, MemorySize::Ram};
		const Range BIOS = {0xBFC00000, MemorySize::Bios};
		const Range SPU = {0x1F801C00, 0x180};
		const Range CDROM = {0x1F801800, 4};
		const Range CACHECONTROL = {0xFFFE0130, 4};
		const Range PAD = {0x1F801040, 16};
		const Range MEMCONTROL = {0x1F801000, 36};
		const Range MEMCONTROL2 = {0x1F801060, 4};
		const Range IRQCONTROL = {0x1F801070, 8};
		const Range GPU = {0x1F801810, 8};
		const Range MDEC = {0x1F801820, 8};
		const Range DMA = {0x1F801080, 0x80};
		const Range TIMERS = {0x1F801100, 0x30};
		const Range EXP1 = {0x1F000000, 0x800000};
		const Range EXP2 = {0x1F802000, 0x88};  // Includes PCSX-Redux expansion registers

		const u32 region_mask[8] = {
			0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0x7fffffff, 0x1fffffff, 0xffffffff, 0xffffffff,
		};

		inline u32 mask(u32 address) { return address & region_mask[address >> 29]; }
	};

}  // namespace Bus

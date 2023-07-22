#include "bus.hpp"
#include "cpu.hpp"

Bus::Bus(Cpu& cpu) : cpu(cpu) {
		try {
				m_ram = new u8[RAM_SIZE];
				m_bios = new u8[BIOS_SIZE];
				m_scratchpad = new u8[SCRATCHPAD_SIZE];
				m_spuram = new u8[SPU_SIZE];
				m_timers = new u8[TIMERS_SIZE];

				m_readPages = new uintptr_t[0x10000];
				m_writePages = new uintptr_t[0x10000];
		} catch (...) {
				Helpers::panic("Error allocating memory for Emulator\n");
		}

		constexpr u32 PAGE_SIZE = 64 * 1024;  // 64Kb Pages

		// RAM Read Pages
		for (auto index = 0; index < 128; index++) {
				const auto pointer = (uintptr_t)&m_ram[index * PAGE_SIZE];
				m_readPages[index + 0x0000] = pointer;  // KUSEG
				m_readPages[index + 0x8000] = pointer;  // KSEG0
				m_readPages[index + 0xA000] = pointer;  // KSEG1
		}
		// RAM Write Pages
		for (auto index = 0; index < 128; index++) {
				const auto pointer = (uintptr_t)&m_ram[index * PAGE_SIZE];
				m_writePages[index + 0x0000] = pointer;  // KUSEG
				m_writePages[index + 0x8000] = pointer;  // KSEG0
				m_writePages[index + 0xA000] = pointer;  // KSEG1
		}
		// BIOS Read Pages
		for (auto index = 0; index < 8; index++) {
				const auto pointer = (uintptr_t)&m_bios[index * PAGE_SIZE];
				m_readPages[index + 0x1FC0] = pointer;  // KUSEG BIOS
				m_readPages[index + 0x9FC0] = pointer;  // KSEG0 BIOS
				m_readPages[index + 0xBFC0] = pointer;  // KSEG1 BIOS
		}
}

void Bus::triggerInterrupt(InterruptType interrupt) {
		m_interruptControl.ISTAT.r |= (1 << std::underlying_type<InterruptType>::type(interrupt));
}

void Bus::init() {

}

void Bus::reset() {
		std::memset(m_ram, 0, RAM_SIZE);
		std::memset(m_scratchpad, 0, SCRATCHPAD_SIZE);
		std::memset(m_memControl, 0, 36);
		std::memset(m_spuram, 0, SPU_SIZE);
		std::memset(m_timers, 0, TIMERS_SIZE);
		m_memControl2 = 0;
}

Bus::~Bus() {
		delete[] m_ram;
		delete[] m_bios;
		delete[] m_scratchpad;
		delete[] m_spuram;
		delete[] m_timers;

		delete[] m_readPages;
		delete[] m_writePages;
}

void Bus::loadBIOS(const std::filesystem::path& path) {
		auto bios = loadBin(path);
		if (bios.size() != BIOS_SIZE) {
				Log::warn("BIOS size is incorrect. Expected 512KB, got {} bytes", bios.size());
				biosLoaded = false;
				return;
		}

		std::copy(bios.begin(), bios.end(), m_bios);
		Log::info("BIOS Loaded");
		biosLoaded = true;
}

void Bus::patchBIOS() {}

void Bus::shellReached() {
		if (sideloading) sideload();
		sideloading = false;
}

void Bus::prepSideload(const std::filesystem::path& path) {
		m_sideloadInfo.load(path);
		if (m_sideloadInfo.ready) sideloading = true;
}

void Bus::sideload() {
		auto address = mask(m_sideloadInfo.address);
		auto size = m_sideloadInfo.size;
		cpu.setPC(m_sideloadInfo.initialPC);
		for (auto i = 0; i < size; i++) {
				m_ram[address + i] = m_sideloadInfo.binary[i];
		}
}
u32 Bus::fetchInstruction(u32 address) {
		const auto page = address >> 16;
		const auto offset = address & 0xFFFF;
		const auto pointer = m_readPages[page];

		cpu.m_cycles +=
				(page == 0xBFC0 || page == 0x9FC0 || page == 0x1FC0) ? BIOS_ACCESS_BIAS : MEM_ACCESS_BIAS;

		// BIOS/RAM Fastmem Reads
		if (pointer != 0) {
				return *(u32*)(pointer + offset);
		}
}

template <typename T>
T Bus::read(u32 address) {
		const auto page = address >> 16;
		const auto offset = address & 0xFFFF;
		const auto pointer = m_readPages[page];

		if (cpu.isCacheIsolated()) {
				return 0;
		}

		cpu.m_cycles +=
				(page == 0xBFC0 || page == 0x9FC0 || page == 0x1FC0) ? BIOS_ACCESS_BIAS : MEM_ACCESS_BIAS;


		// BIOS/RAM Fastmem Reads
		if (pointer != 0) {
				return *(T*)(pointer + offset);
		}

		// Slow Reads to Scratchpad
		if ((page == 0x1F80 || page == 0x9F80) && offset < SCRATCHPAD_SIZE) {
				return *(T*)(m_scratchpad + offset);
		} else if (page == 0xBF80 && offset < SCRATCHPAD_SIZE) {
				Helpers::panic("Scratchpad read from KSEG1 prohibited\n");
		}

		// Slow Reads to MMIO
		auto hw_address = mask(address);

		// TEMP TIMER2 STUB
		if (hw_address == 0x1f801120) {
				if (std::is_same<T, u16>()) {
						return 0x000016b0;
				}
		}

		if (INTERRUPT_CONTROL.contains(hw_address)) {
				auto offset = INTERRUPT_CONTROL.offset(hw_address);
				if (offset == 0) {
						return static_cast<u16>(m_interruptControl.ISTAT.r);
				} else {
						return static_cast<u16>(m_interruptControl.IMASK.r);
				}
		}

		if (CACHE_CONTROL.contains(hw_address)) {
				return static_cast<T>(m_cacheControl.r);
		}

		// 8 bit
		if constexpr (std::is_same_v<T, u8>) {
				if (CDROM.contains(hw_address)) {
						auto offset = CDROM.offset(hw_address);
						//return m_emulator->cdrom->read(offset);
				}
				if (PAD_MEMCARD.contains(hw_address)) {
						auto offset = PAD_MEMCARD.offset(hw_address);
						//return m_emulator->sio->read<T>(offset);
				}
				if (EXPANSION_1.contains(hw_address)) {
						return 0xff;
				}
				if (EXPANSION_2.contains(hw_address)) {
						return 0xff;
				}
				// 16 bit
		} else if constexpr (std::is_same_v<T, u16>) {
				if (TIMERS.contains(hw_address)) {
						//return m_emulator->timers->read(hw_address);
				}
				if (PAD_MEMCARD.contains(hw_address)) {
						auto offset = PAD_MEMCARD.offset(hw_address);
						//return m_emulator->sio->read<T>(offset);
				}
				if (SPU.contains(hw_address)) {
						auto offset = SPU.offset(hw_address);
						return *(u16*)(m_spuram + offset);
				}
				// Handle as 2 16 bit reads
				if (CDROM.contains(hw_address)) {
						auto offset = CDROM.offset(hw_address);
						//u8 value1 = m_emulator->cdrom->read(offset);
						//u8 value2 = m_emulator->cdrom->read(offset);
						//return (value1 << 8) | value2;
				}
				// 32 bit
		} else if constexpr (std::is_same_v<T, u32>) {
				if (TIMERS.contains(hw_address)) {
						//return static_cast<u16>(m_emulator->timers->read(hw_address));
				}
				if (MEM_CONTROL.contains(hw_address)) {
						auto offset = MEM_CONTROL.offset(hw_address);
						return *(T*)(m_memControl + offset);
				}
				if (MEM_CONTROL2.contains(hw_address)) {
						return m_memControl2;
				}
				if (RAMSIZE.contains(hw_address)) {
						return static_cast<T>(m_ramSize.r);
				}
				if (DMA.contains(hw_address)) {
						auto offset = DMA.offset(hw_address);
						//return m_emulator->dma->read(offset);
				}
				// only 32 bit writes
				if (GPU.contains(hw_address)) {
						auto offset = GPU.offset(hw_address);
						//return m_emulator->gpu->read(offset);
				}
				if (PAD_MEMCARD.contains(hw_address)) {
						auto offset = PAD_MEMCARD.offset(hw_address);
						//return m_emulator->sio->read<T>(offset);
				}
		}
		Log::warn("Unknown read at address: {:#08x}\n", address);
		return 0;
}

template <typename T>
void Bus::write(u32 address, T value) {
		const auto page = address >> 16;
		const auto offset = address & 0xFFFF;
		const auto pointer = m_writePages[page];

		if (cpu.isCacheIsolated()) {
				return;
		}

		cpu.m_cycles += MEM_ACCESS_BIAS;


		// BIOS/RAM Fastmem Writes
		if (pointer != 0) {
				*(T*)(pointer + offset) = value;
				return;
		}

		// Slow Reads to Scratchpad
		if ((page == 0x1F80 || page == 0x9F80) && offset < SCRATCHPAD_SIZE) {
				*(T*)(m_scratchpad + offset) = value;
				return;
		} else if (page == 0xBF80 && offset < SCRATCHPAD_SIZE) {
				Helpers::panic("Scratchpad write from KSEG1 prohibited\n");
		}

		// Slow writes to MMIO
		auto hw_address = mask(address);

		if (INTERRUPT_CONTROL.contains(hw_address)) {
				auto offset = INTERRUPT_CONTROL.offset(hw_address);
				if (offset == 0) {
						m_interruptControl.ISTAT.r &= static_cast<u16>(value & 0x7FF);
				} else {
						m_interruptControl.IMASK.r = static_cast<u16>(value & 0x7FF);
				}
				return;
		}

		// 8 bit
		if constexpr (std::is_same_v<T, u8>) {

				if (CDROM.contains(hw_address)) {
						auto offset = CDROM.offset(hw_address);
						//m_emulator->cdrom->write(offset, value);
						return;
				}
				if (PAD_MEMCARD.contains(hw_address)) {
						auto offset = PAD_MEMCARD.offset(hw_address);
						//m_emulator->sio->write<T>(offset, value);
						return;
				}
				if (EXPANSION_2.contains(hw_address)) {
						return;
				}

		} else if constexpr (std::is_same_v<T, u16>) {

				if (PAD_MEMCARD.contains(hw_address)) {
						auto offset = PAD_MEMCARD.offset(hw_address);
						//m_emulator->sio->write<T>(offset, value);
						return;
				}
				if (TIMERS.contains(hw_address)) {
						//m_emulator->timers->write(hw_address, value);
						return;
				}
				if (SPU.contains(hw_address)) {
						auto offset = SPU.offset(hw_address);
						*(u16*)(m_spuram + offset) = value;
						return;
				}

		} else if constexpr (std::is_same_v<T, u32>) {

				if (TIMERS.contains(hw_address)) {
						//m_emulator->timers->write(hw_address, static_cast<u16>(value));
						return;
				}
				if (MEM_CONTROL.contains(hw_address)) {
						auto offset = MEM_CONTROL.offset(hw_address);
						*(u32*)(m_memControl + offset) = value;
						return;
				}
				if (MEM_CONTROL2.contains(hw_address)) {
						m_memControl2 = value;
						return;
				}
				if (RAMSIZE.contains(hw_address)) {
						m_ramSize.r = value;
						return;
				}
				if (CACHE_CONTROL.contains(hw_address)) {
						m_cacheControl.r = value;
						return;
				}
				if (DMA.contains(hw_address)) {
						auto offset = DMA.offset(hw_address);
						//m_emulator->dma->write(offset, value);
						return;
				}
				if (GPU.contains(hw_address)) {
						auto offset = GPU.offset(hw_address);
						//m_emulator->gpu->write(offset, value);
						return;
				}
				if (EXPANSION_1.contains(hw_address)) {
						return;
				}

		}

		Log::warn("Unknown write at address: {:#08x} - value {:#08x}\n", address, value);
}

template u8 Bus::read<u8>(u32);
template u16 Bus::read<u16>(u32);
template u32 Bus::read<u32>(u32);

template void Bus::write<u8>(u32, u8);
template void Bus::write<u16>(u32, u16);
template void Bus::write<u32>(u32, u32);

#include "cpu.hpp"

#include "bus/bus.hpp"
#include "support/log.hpp"

namespace Cpu {

	Cpu::Cpu(Bus::Bus& bus) : bus(bus) { reset(); }

	Cpu::~Cpu() {}

	void Cpu::reset() {
		for (auto& reg : regs.gpr) {
			reg = 0;
		}

		regs.cop0.status = 0;
		regs.cop0.cause = 0;
		regs.cop0.bva = 0;
		regs.cop0.epc = 0;

		delayedLoad.reset();
		memoryLoad.reset();
		writeBack.reset();

		PC = RESET_VECTOR;
		nextPC = PC + 4;
		currentPC = PC;
		branch = false;
		branchTaken = false;
		delaySlot = false;
		branchTakenDelaySlot = false;
		totalCycles = 0;
		cycleTarget = 0;
		ttyBuffer.clear();
	}

	void Cpu::step() {
		// Fetch
		m_instruction = bus.read<u32>(PC);

		if ((PC % 4) != 0) {
			Log::warn("Unaligned PC {:#x}\n", PC);
			ExceptionHandler(BadLoadAddress);
			return;
		}

		// Advance PC
		currentPC = PC;
		PC = nextPC;
		nextPC += 4;

		// Update delay slot info
		delaySlot = branch;
		branchTakenDelaySlot = branchTaken;
		branchTaken = false;
		branch = false;

		// Get opcode function from LUT and execute
		const auto func = basic[m_instruction.opcode];
		(this->*func)();

		// Do memory access
		if (delayedLoad.reg != memoryLoad.reg) {
			regs.gpr[memoryLoad.reg] = memoryLoad.value;
		}
		memoryLoad = delayedLoad;
		delayedLoad.reset();

		// Write back registers
		regs.gpr[writeBack.reg] = writeBack.value;
		writeBack.reset();

		handleKernelCalls();
		checkInterrupts();

		addCycles(Bus::CycleBias::CPI);
	}

	void Cpu::handleKernelCalls() {
		const u32 pc = PC & 0x1FFFFF;
		const u32 func = regs.gpr[9];

		if (pc == 0xB0) {
			switch (func) {
				// putchar
				case 0x3D: {
					const char c = regs.gpr[A0];
					if (c == '\r') break;
					if (c == '\n') {
						Log::info("{}\n", ttyBuffer);
						ttyBuffer.clear();
						break;
					}
					ttyBuffer += c;
				}
			}
		}
	}

	void Cpu::triggerInterrupt() {
		regs.cop0.cause &= static_cast<u32>(~0x400);
		if (bus.isIRQPending()) {
			regs.cop0.cause |= static_cast<u32>(0x400);
		}
	}

	void Cpu::checkInterrupts() {
		bool iec = (regs.cop0.status & 1);
		u8 im = (regs.cop0.status >> 8) & 0xFF;
		u8 ip = (regs.cop0.cause >> 8) & 0xFF;

		if (iec && (im & ip) > 0) {
			ExceptionHandler(Exception::Interrupt);
		}
	}

	void Cpu::ExceptionHandler(Exception cause, u32 cop) {
		auto& sr = regs.cop0.status;
		auto vector = ExceptionHandlerAddr[Helpers::isBitSet(sr, 22)];

		auto mode = sr & 0x3F;
		sr = (sr & ~0x3f);
		sr = sr | ((mode << 2) & 0x3F);

		regs.cop0.cause &= ~0x7C;
		regs.cop0.cause = static_cast<u32>(cause) << 2;

		if (cause == Exception::Interrupt) {
			regs.cop0.epc = PC;
			delaySlot = branch;
			branchTakenDelaySlot = branchTaken;
		} else {
			regs.cop0.epc = currentPC;
		}

		if (delaySlot) {
			regs.cop0.epc -= 4;
			regs.cop0.cause |= (1 << 31);
		} else {
			regs.cop0.cause &= ~(1 << 31);
		}

		setPC(vector);
	}

	void Cpu::Branch(bool link) {}

	void Cpu::Unknown() {}
	void Cpu::Special() {}
	void Cpu::REGIMM() {}
	void Cpu::J() {}
	void Cpu::JAL() {}
	void Cpu::BEQ() {}
	void Cpu::NOP() {}
	void Cpu::ORI() {}
	void Cpu::SW() {}
	void Cpu::LUI() {}
	void Cpu::SLL() {}
	void Cpu::ADDIU() {}
	void Cpu::ADDI() {}
	void Cpu::ADD() {}
	void Cpu::ADDU() {}
	void Cpu::AND() {}
	void Cpu::ANDI() {}
	void Cpu::BGTZ() {}
	void Cpu::BLEZ() {}
	void Cpu::BNE() {}
	void Cpu::BREAK() {}
	void Cpu::CFC2() {}
	void Cpu::COP0() {}
	void Cpu::COP2() {}
	void Cpu::CTC2() {}
	void Cpu::DIV() {}
	void Cpu::DIVU() {}
	void Cpu::JALR() {}
	void Cpu::JR() {}
	void Cpu::LB() {}
	void Cpu::LBU() {}
	void Cpu::LH() {}
	void Cpu::LHU() {}
	void Cpu::LW() {}
	void Cpu::LWC2() {}
	void Cpu::LWL() {}
	void Cpu::LWR() {}
	void Cpu::MFC0() {}
	void Cpu::MFC2() {}
	void Cpu::MFHI() {}
	void Cpu::MFLO() {}
	void Cpu::MTC0() {}
	void Cpu::MTC2() {}
	void Cpu::MTHI() {}
	void Cpu::MTLO() {}
	void Cpu::MULT() {}
	void Cpu::MULTU() {}
	void Cpu::NOR() {}
	void Cpu::OR() {}
	void Cpu::RFE() {}
	void Cpu::SB() {}
	void Cpu::SH() {}
	void Cpu::SLLV() {}
	void Cpu::SLT() {}
	void Cpu::SLTI() {}
	void Cpu::SLTIU() {}
	void Cpu::SLTU() {}
	void Cpu::SRA() {}
	void Cpu::SRAV() {}
	void Cpu::SRL() {}
	void Cpu::SRLV() {}
	void Cpu::SUB() {}
	void Cpu::SUBU() {}
	void Cpu::SWC2() {}
	void Cpu::SWL() {}
	void Cpu::SWR() {}
	void Cpu::SYSCALL() {}
	void Cpu::XOR() {}
	void Cpu::XORI() {}
	void Cpu::GTEMove() {}
	void Cpu::AVSZ3() {}
	void Cpu::AVSZ4() {}
	void Cpu::CC() {}
	void Cpu::CDP() {}
	void Cpu::DCPL() {}
	void Cpu::DPCS() {}
	void Cpu::DPCT() {}
	void Cpu::GPF() {}
	void Cpu::GPL() {}
	void Cpu::INTPL() {}
	void Cpu::MVMVA() {}
	void Cpu::NCCS() {}
	void Cpu::NCCT() {}
	void Cpu::NCDS() {}
	void Cpu::NCDT() {}
	void Cpu::NCLIP() {}
	void Cpu::NCS() {}
	void Cpu::NCT() {}
	void Cpu::OP() {}
	void Cpu::RTPS() {}
	void Cpu::RTPT() {}
	void Cpu::SQR() {}

}  // namespace Cpu

#include "cpu.hpp"

#include "support/log.hpp"

namespace Cpu {

	Cpu::Cpu() {}

	Cpu::~Cpu() {}

	void Cpu::reset() {
		m_regs.gpr.reset();
		m_regs.cop0.reset();
		m_delayedLoad.reset();
		m_memoryLoad.reset();
		m_writeBack.reset();

		m_pc = RESET_VECTOR;
		m_next_pc = m_pc + 4;
		m_current_pc = m_pc;
		m_branch = false;
		m_branchTaken = false;
		m_delaySlot = false;
		m_branchTakenDelaySlot = false;
		totalCycles = 0;
		cyclesToRun = 0;
		ttyBuffer.clear();
	}

	void Cpu::step() {}

	void Cpu::fetch() {}

	void Cpu::execute() {}

	void Cpu::memory() {
		if (m_delayedLoad.reg != m_memoryLoad.reg) {
			m_regs.gpr[m_memoryLoad.reg] = m_memoryLoad.value;
		}
		m_memoryLoad = m_delayedLoad;
		m_delayedLoad.reset();
	}

	void Cpu::writeback() {
		m_regs.gpr[m_writeBack.reg] = m_writeBack.value;
		m_writeBack.reset();
	}

	void Cpu::handleKernelCalls() {
		const u32 pc = m_pc & 0x1FFFFF;
		const u32 func = m_regs.gpr[9];

		if (pc == 0xB0) {
			switch (func) {
				// putchar
				case 0x3D: {
					const char c = m_regs.gpr[A0];
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

	void Cpu::checkInterrupts() {}

	void Cpu::ExceptionHandler(Exception cause, u32 cop) {}

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

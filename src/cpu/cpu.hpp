#pragma once
#include <array>
#include <cassert>
#include <string>

#include "BitField.hpp"
#include "support/helpers.hpp"

namespace Bus {
	class Bus;
}

namespace Cpu {
	// clang-format off
	enum : u32 {
		ZERO, AT, V0, V1, A0, A1, A2, A3, T0, T1, T2, T3, T4, T5, T6, T7,
		S0, S1, S2, S3, S4, S5, S6, S7, T8, T9, K0, K1, GP, SP, FP, RA,
		HI, LO
	};

	enum : u32 { R0, R1, R2, BPC, R4, BDA, TAR, DCIC, BVA, BDAM, R10, BPCM, SR, CAUSE, EPC, PRID };
	// clang-format on

	union Instruction {
		u32 code;

		BitField<26, 6, u32> opcode;

		union {
			BitField<21, 5, u32> rs;
			BitField<16, 5, u32> rt;
			BitField<11, 5, u32> rd;
		};

		union {
			BitField<0, 16, u32> imm;
		};

		union {
			BitField<0, 16, s32> immse;
		};

		union {
			BitField<0, 26, u32> tar;
		};

		BitField<16, 1, u32> bgez;
		BitField<20, 1, u32> link;

		BitField<6, 5, u32> sa;
		BitField<0, 6, u32> fn;

		Instruction& operator=(const u32 value) {
			code = value;
			return *this;
		}
	};

	struct Writeback {
		u32 reg;
		u32 value;

		inline void reset() { reg = value = 0; }

		inline void set(u32 rt, u32 val) {
			reg = rt;
			value = val;
		}
	};

	enum Exception : u32 {
		Interrupt = 0x0,
		Syscall = 0x8,
		Break = 0x9,
		CopError = 0xB,
		Overflow = 0xC,
		BadLoadAddress = 0x4,
		BadStoreAddress = 0x5,
		IllegalInstruction = 0xA,
	};

	struct Cop0Regs {
		u32 cause;
		u32 status;
		u32 bva;
		u32 epc;
	};

	struct Regs {
		std::array<u32, 34> gpr;
		Cop0Regs cop0;

		inline u32 get(size_t index) { return gpr[index]; }
		inline void set(size_t index, u32 value) { gpr[index] = value; }
	};

	class Cpu {
		using funcPtr = void (Cpu::*)();

	  public:
		Cpu(Bus::Bus& bus);
		~Cpu();

		void reset();
		void step();
		void run();

		[[nodiscard]] auto getPC() const -> u32 { return PC; }
		void setPC(u32 pc) {
			PC = pc;
			nextPC = pc + 4;
		}

		[[nodiscard]] bool isCacheIsolated() const { return regs.cop0.status & (1 << 16); }

		[[nodiscard]] auto getTotalCycles() const -> Cycles { return totalCycles; }
		Cycles& getCycleRef() { return totalCycles; }
		Cycles& getCycleTargetRef() { return cycleTarget; }
		[[nodiscard]] auto getCycleTarget() const -> Cycles { return cycleTarget; }
		void setCycleTarget(Cycles cycles) { cycleTarget = cycles; }
		void addCycles(Cycles cycles) { totalCycles += cycles; }
		void triggerInterrupt();

	  private:
		Bus::Bus& bus;
		void handleKernelCalls();
		void checkInterrupts();

		Instruction instruction{0};
		Regs regs;

		Writeback delayedLoad;
		Writeback memoryLoad;
		Writeback writeBack;

		Cycles totalCycles;
		Cycles cycleTarget;

		u32 PC;
		u32 nextPC;
		u32 currentPC;
		bool branch;
		bool branchTaken;
		bool delaySlot;
		bool branchTakenDelaySlot;

		std::string ttyBuffer;
		const u32 RESET_VECTOR = 0xbfc00000;
		const u32 SHELL_PC = 0x80030000;
		const u32 ExceptionHandlerAddr[2] = {0x80000080, 0xbfc00180};

		void ExceptionHandler(Exception cause, u32 cop = 0);
		void Branch(bool link = false);
		void Unknown();
		void Special();
		void REGIMM();
		void J();
		void JAL();
		void BEQ();
		void NOP();
		void ORI();
		void SW();
		void LUI();
		void SLL();
		void ADDIU();
		void ADDI();

		void ADD();
		void ADDU();
		void AND();
		void ANDI();
		void BGTZ();
		void BLEZ();
		void BNE();
		void BREAK();
		void CFC2();
		void COP0();
		void COP2();
		void CTC2();
		void DIV();
		void DIVU();
		void JALR();
		void JR();
		void LB();
		void LBU();
		void LH();
		void LHU();
		void LW();
		void LWC2();
		void LWL();
		void LWR();
		void MFC0();
		void MFC2();
		void MFHI();
		void MFLO();
		void MTC0();
		void MTC2();
		void MTHI();
		void MTLO();
		void MULT();
		void MULTU();
		void NOR();
		void OR();
		void RFE();
		void SB();
		void SH();
		void SLLV();
		void SLT();
		void SLTI();
		void SLTIU();
		void SLTU();
		void SRA();
		void SRAV();
		void SRL();
		void SRLV();
		void SUB();
		void SUBU();
		void SWC2();
		void SWL();
		void SWR();
		void SYSCALL();
		void XOR();
		void XORI();

		void GTEMove();
		void AVSZ3();
		void AVSZ4();
		void CC();
		void CDP();
		void DCPL();
		void DPCS();
		void DPCT();
		void GPF();
		void GPL();
		void INTPL();
		void MVMVA();
		void NCCS();
		void NCCT();
		void NCDS();
		void NCDT();
		void NCLIP();
		void NCS();
		void NCT();
		void OP();
		void RTPS();
		void RTPT();
		void SQR();

		const funcPtr basic[64] = {
			&Cpu::Special, &Cpu::REGIMM,  &Cpu::J,       &Cpu::JAL,     &Cpu::BEQ,     &Cpu::BNE,     &Cpu::BLEZ,    &Cpu::BGTZ,
			&Cpu::ADDI,    &Cpu::ADDIU,   &Cpu::SLTI,    &Cpu::SLTIU,   &Cpu::ANDI,    &Cpu::ORI,     &Cpu::XORI,    &Cpu::LUI,
			&Cpu::COP0,    &Cpu::Unknown, &Cpu::COP2,    &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::LB,      &Cpu::LH,      &Cpu::LWL,     &Cpu::LW,      &Cpu::LBU,     &Cpu::LHU,     &Cpu::LWR,     &Cpu::Unknown,
			&Cpu::SB,      &Cpu::SH,      &Cpu::SWL,     &Cpu::SW,      &Cpu::Unknown, &Cpu::Unknown, &Cpu::SWR,     &Cpu::Unknown,
			&Cpu::Unknown, &Cpu::Unknown, &Cpu::LWC2,    &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::Unknown, &Cpu::Unknown, &Cpu::SWC2,    &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,

		};

		const funcPtr special[64] = {
			&Cpu::SLL,     &Cpu::Unknown, &Cpu::SRL,     &Cpu::SRA,     &Cpu::SLLV,    &Cpu::Unknown, &Cpu::SRLV,    &Cpu::SRAV,
			&Cpu::JR,      &Cpu::JALR,    &Cpu::Unknown, &Cpu::Unknown, &Cpu::SYSCALL, &Cpu::BREAK,   &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::MFHI,    &Cpu::MTHI,    &Cpu::MFLO,    &Cpu::MTLO,    &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::MULT,    &Cpu::MULTU,   &Cpu::DIV,     &Cpu::DIVU,    &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::ADD,     &Cpu::ADDU,    &Cpu::SUB,     &Cpu::SUBU,    &Cpu::AND,     &Cpu::OR,      &Cpu::XOR,     &Cpu::NOR,
			&Cpu::Unknown, &Cpu::Unknown, &Cpu::SLT,     &Cpu::SLTU,    &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
		};

		const funcPtr gte[64] = {
			&Cpu::GTEMove, &Cpu::RTPS,    &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::NCLIP,   &Cpu::Unknown,
			&Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::OP,      &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::DPCS,    &Cpu::INTPL,   &Cpu::MVMVA,   &Cpu::NCDS,    &Cpu::CDP,     &Cpu::Unknown, &Cpu::NCDT,    &Cpu::Unknown,
			&Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::NCCS,    &Cpu::CC,      &Cpu::Unknown, &Cpu::NCS,     &Cpu::Unknown,
			&Cpu::NCT,     &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::SQR,     &Cpu::DCPL,    &Cpu::DPCT,    &Cpu::Unknown, &Cpu::Unknown, &Cpu::AVSZ3,   &Cpu::AVSZ4,   &Cpu::Unknown,
			&Cpu::RTPT,    &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown,
			&Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::Unknown, &Cpu::GPF,     &Cpu::GPL,     &Cpu::NCCT,
		};
	};
}  // namespace Cpu

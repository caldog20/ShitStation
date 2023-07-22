#pragma once
#include <string>
#include "BitField.hpp"
#include "support/utils.hpp"

class Bus;

enum Exception {
		Interrupt = 0x0,
		Syscall = 0x8,
		Break = 0x9,
		CopError = 0xB,
		Overflow = 0xC,
		BadLoadAddress = 0x4,
		BadStoreAddress = 0x5,
		IllegalInstruction = 0xA,
};

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

		void operator=(const u32 value) { code = value; }
};

// General Purpose
union Gpr {
		struct {
				u32 zero, at, v0, v1, a0, a1, a2, a3;
				u32 t0, t1, t2, t3, t4, t5, t6, t7;
				u32 s0, s1, s2, s3, s4, s5, s6, s7;
				u32 t8, t9, k0, k1, gp, sp, fp, ra;
		};
		u32 r[32];
};

union Cop0Registers {
		u32 r[64];
		struct {
				u32 r0;
				u32 r1;
				u32 r2;
				u32 bpc;
				u32 r4;
				u32 bda;
				u32 tar;
				u32 dcic;
				u32 bva;
				u32 bdam;
				u32 r10;
				u32 bpcm;
				u32 sr;
				u32 cause;
				u32 epc;
				u32 prid;
				u32 unused[32];
		};
};

struct Vector3 {
		s16 x, y, z, pad;
};

struct Vector2 {
		s16 x, y;
};

struct BGRC {
		u8 b, g, r, c;
};

struct RGBC {
		u8 r, g, b, c;
};

struct SZ {
		u16 s, pad;
};

union GTEDataRegisters {
		u32 r[32];
		struct {
				Vector3 v0, v1, v2;
				BGRC rgb;
				u32 otz;
				s32 ir0, ir1, ir2, ir3;
				Vector2 sxy0, sxy1, sxy2, sxyp;
				SZ sz0, sz1, sz2, sz3;
				RGBC rgb0, rgb1, rgb2;
				u32 res1;
				s32 mac0, mac1, mac2, mac3;
				u32 irgb, orgb;
				s32 lzcs, lzcr;
		};
};

union GTEControlRegisters {
		u32 r[32];
		struct {
				s16 r11, r12, r13, r21, r22, r23, r31, r32;
				s32 r33;
				s32 trX, trY, trZ;
				s16 l11, l12, l13, l21, l22, l23, l31, l32;
				s32 l33;
				s32 rbk, bbk, gbk;
				s16 lr1, lr2, lr3, lg1, lg2, lg3, lb1, lb2;
				s32 lb3;
				s32 rfc, gfc, bfc;
				s32 ofx, ofy;
				s32 h;
				s32 dqa;
				s32 dqb;
				s32 zf3;
				s32 zf4;
				u32 flag;
		};
};

struct Writeback {
		u32 reg;
		u32 value;

		void reset() { reg = value = 0; }

		void set(u32 rt, u32 val) {
				reg = rt;
				value = val;
		}
};

struct Registers {
		Gpr gpr;
		u32 hi, lo;
		Cop0Registers cop0;
		GTEControlRegisters gteCtrl;
		GTEDataRegisters gteData;

		Writeback delayedLoad;
		Writeback memoryLoad;
		Writeback writeBack;

		u32 opcode;

		Registers() { reset(); }

		void reset() {
				std::memset(&gpr, 0, sizeof(gpr));
				std::memset(&cop0, 0, sizeof(cop0));
				std::memset(&gteCtrl, 0, sizeof(gteCtrl));
				std::memset(&gteData, 0, sizeof(gteData));

				hi = lo = 0;

				delayedLoad.reset();
				memoryLoad.reset();
				writeBack.reset();
		}

		void set(u32 reg, u32 value) {
				writeBack.reg = reg;
				writeBack.value = value;
		}

		void writeback() {
				gpr.r[writeBack.reg] = writeBack.value;
				writeBack.reset();
		}

		void pendingLoad(u32 rt, u32 val) { delayedLoad.set(rt, val); }

		inline void zero() { gpr.r[0] = 0; }

		inline void setcop(u32 reg, u32 value) { cop0.r[reg] = value; }

		inline void setGteData(u32 reg, u32 value) { gteData.r[reg] = value; }
		inline void setGteCtrl(u32 reg, u32 value) { gteCtrl.r[reg] = value; }

		inline u32 get(u32 reg) { return gpr.r[reg]; }
		inline u32 getcop(u32 reg) { return cop0.r[reg]; }
		inline u32 getGteData(u32 reg) { return gteData.r[reg]; }
		inline u32 getGteCtrl(u32 reg) { return gteCtrl.r[reg]; }
};

class Cpu {
		using funcPtr = void (Cpu::*)();

	public:
		explicit Cpu(Bus& bus);

		void init();
		void reset();

		void run();
		void step();
		inline void fetch();
		inline void execute();
		inline void memory();
		inline void writeback();
		void handleKernelCalls();
		void handleInterrupts();
		void exception(Exception cause) { ExceptionHandler(cause); }
		void setPC(u32 pc) {
				m_pc = pc;
				m_next_pc = pc + 4;
		}

		[[nodiscard]] bool isCacheIsolated() const { return (m_regs.cop0.sr & 0x10000); }

		Cycles& cycleRef() { return m_cycles; }

		u32 m_pc;
		u32 m_next_pc;
		u32 m_current_pc;
		Cycles m_cycles;
		u32 m_cycleTarget;
		u32 m_prevCycles;
		bool m_branch;
		bool m_branchTaken;
		bool m_delaySlot;
		bool m_branchTakenDelaySlot;

		Registers m_regs;
		Instruction m_instruction{0};

		const u32 RESET_VECTOR = 0xbfc00000;
		const u32 SHELL_PC = 0x80010000;

	private:
		Bus& bus;
		std::string m_biosTtyBuffer;
		u32 ExceptionHandlerAddr[2] = {0x80000080, 0xbfc00180};

		// Instructions and LUTS

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

#include "cpu.hpp"
#include "bus.hpp"

Cpu::Cpu(Bus& bus) : bus(bus) {
		reset();
}

void Cpu::reset() {
		m_pc = RESET_VECTOR;
		m_next_pc = m_pc + 4;
		m_current_pc = m_pc;
		m_cycles = 0;
		m_cycleTarget = 0;
		m_prevCycles = 0;
		m_regs.reset();
		m_branch = false;
		m_branchTaken = false;
		m_branchTakenDelaySlot = false;
		m_delaySlot = false;
}

void Cpu::run() {
		fetch();
		execute();
		memory();
		writeback();

		handleKernelCalls();
		handleInterrupts();
		m_cycles += BIAS;
}

void Cpu::step() {
		fetch();
		execute();
		memory();
		writeback();

		handleKernelCalls();
		handleInterrupts();
		m_cycles += BIAS;
}

inline void Cpu::fetch() {
		if (m_pc == SHELL_PC) bus.shellReached();
		m_instruction = bus.fetchInstruction(m_pc);
		//    logMnemonic();
		//        Log<Log::Cpu>("PC: {:#x} Instruction: {:#x}\n", m_pc, m_instruction.code);
		if ((m_pc % 4) != 0) {
				Log::warn("Unaligned PC {:#x}\n", m_pc);
				ExceptionHandler(BadLoadAddress);
				return;
		}

		m_current_pc = m_pc;
		m_pc = m_next_pc;
		m_next_pc += 4;

		m_delaySlot = m_branch;
		m_branchTakenDelaySlot = m_branchTaken;
		m_branchTaken = false;
		m_branch = false;
}

inline void Cpu::execute() {
		const auto func = basic[m_instruction.opcode];
		(this->*func)();
}

inline void Cpu::memory() {
		if (m_regs.delayedLoad.reg != m_regs.memoryLoad.reg) {
				m_regs.gpr.r[m_regs.memoryLoad.reg] = m_regs.memoryLoad.value;
		}
		m_regs.memoryLoad = m_regs.delayedLoad;
		m_regs.delayedLoad.reset();
}

inline void Cpu::writeback() { m_regs.writeback(); }

void Cpu::handleKernelCalls() {
		const u32 pc = m_pc & 0x1FFFFF;
		const u32 call = m_regs.gpr.r[9] & 0xFF;
		auto& regs = m_regs.gpr;

		if (pc == 0xB0 && call == 0x3D) {
				char c = regs.a0;
				if (c == '\r') return;
				if (c == '\n') {
						if (m_biosTtyBuffer.empty()) {
								Log::info("\n");
								return;
						}
						Log::info("{}\n", m_biosTtyBuffer);
						m_biosTtyBuffer.clear();
						return;
				}
				m_biosTtyBuffer += std::string(1, c);
		}
}

void Cpu::handleInterrupts() {
		if (bus.irqActive()) {
				m_regs.cop0.cause |= 0x400;
		} else {
				m_regs.cop0.cause &= ~0x400;
		}

		bool iec = (m_regs.cop0.sr & 0x1) == 1;
		u32 im = (m_regs.cop0.sr >> 8) & 0xFF;
		u32 ip = (m_regs.cop0.cause >> 8) & 0xFF;

		if (iec && (im & ip) > 0) {
				ExceptionHandler(Exception::Interrupt);
		}
}

void Cpu::ExceptionHandler(Exception cause, u32 cop) {
		auto& sr = m_regs.cop0.sr;

		bool handler = Helpers::isBitSet(sr, 22);

		u32 handler_address = ExceptionHandlerAddr[handler];

		sr = (sr & ~0x3f) | ((sr & 0x3f) << 2) & 0x3f;

		m_regs.cop0.cause = static_cast<u32>(cause) << 2;

		if (cause == Exception::Interrupt) {
				m_regs.cop0.epc = m_pc;
				m_delaySlot = m_branch;
				m_branchTakenDelaySlot = m_branchTaken;
		} else {
				m_regs.cop0.epc = m_current_pc;
		}

		if (m_delaySlot) {
				m_regs.cop0.epc -= 4;
				m_regs.cop0.cause |= (1 << 31);
				m_regs.cop0.tar = m_pc;

				if (m_branchTakenDelaySlot) {
						m_regs.cop0.cause |= (1 << 30);
				}
		}

		m_pc = handler_address;
		m_next_pc = m_pc + 4;
}

void Cpu::RFE() {
		if ((m_instruction.code & 0x3f) != 0x10) {
				Log::warn("RFE: Unmatched cop0 instruction\n");
				return;
		}
		u32 mode = m_regs.cop0.sr & 0x3F;
		m_regs.cop0.sr &= ~(u32)0xF;
		m_regs.cop0.sr |= mode >> 2;
}

void Cpu::Special() {
		const auto f = special[m_instruction.fn];
		(this->*f)();
}

void Cpu::SYSCALL() { ExceptionHandler(Exception::Syscall); }
void Cpu::BREAK() { ExceptionHandler(Exception::Break); }

void Cpu::NOP() {}

//// LOAD STORE INSTRUCTIONS

void Cpu::LUI() { m_regs.set(m_instruction.rt, m_instruction.imm << 16); }

void Cpu::LB() {
		u32 addr = m_regs.get(m_instruction.rs) + m_instruction.immse;
		m_regs.pendingLoad(m_instruction.rt, Helpers::signExtend<u32, u8>(bus.read<u8>(addr)));
}

void Cpu::LBU() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;
		m_regs.pendingLoad(m_instruction.rt, bus.read<u8>(address));
}

void Cpu::LH() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;
		if (address % 2 != 0) {
				m_regs.cop0.bva = address;
				ExceptionHandler(Exception::BadLoadAddress);
				return;
		}
		m_regs.pendingLoad(m_instruction.rt, Helpers::signExtend<u32, u16>(bus.read<u16>(address)));
}

void Cpu::LHU() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;
		if (address % 2 != 0) {
				m_regs.cop0.bva = address;
				ExceptionHandler(Exception::BadLoadAddress);
				return;
		}
		m_regs.pendingLoad(m_instruction.rt, bus.read<u16>(address));
}

void Cpu::LW() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;

		if (address % 4 != 0) {
				m_regs.cop0.bva = address;
				ExceptionHandler(Exception::BadLoadAddress);
				return;
		}
		m_regs.pendingLoad(m_instruction.rt, bus.read<u32>(address));
}

void Cpu::LWL() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;

		u32 addr_aligned = address & ~3;
		u32 value = bus.read<u32>(addr_aligned);

		u32 final = 0;

		u32 pending = m_regs.get(m_instruction.rt);

		if (m_instruction.rt == m_regs.memoryLoad.reg) {
				pending = m_regs.memoryLoad.value;
		}

		switch (address & 3) {
				case 0: {
						final = (pending & 0x00ffffff) | (value << 24);
						break;
				}
				case 1: {
						final = (pending & 0xffff) | (value << 16);
						break;
				}
				case 2: {
						final = (pending & 0xff) | (value << 8);
						break;
				}
				case 3:
						final = value;
						break;
		}
		m_regs.pendingLoad(m_instruction.rt, final);
}

void Cpu::LWR() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;

		u32 addr_aligned = address & 0xFFFFFFFC;
		u32 value = bus.read<u32>(addr_aligned);

		u32 final = 0;

		u32 pending = m_regs.get(m_instruction.rt);
		if (m_instruction.rt == m_regs.memoryLoad.reg) {
				pending = m_regs.memoryLoad.value;
		}

		switch (address & 3) {
				case 0: {
						final = value;
						break;
				}
				case 1: {
						final = (pending & 0xff000000) | (value >> 8);
						break;
				}
				case 2: {
						final = (pending & 0xffff0000) | (value >> 16);
						break;
				};
				case 3: {
						final = (pending & 0xffffff00) | (value >> 24);
						break;
				}
		}

		m_regs.pendingLoad(m_instruction.rt, final);
}

void Cpu::SB() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;
		u8 value = m_regs.get(m_instruction.rt);
		bus.write<u8>(address, value);
}

void Cpu::SH() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;

		if (address % 2 != 0) {
				m_regs.cop0.bva = address;
				ExceptionHandler(Exception::BadStoreAddress);
				return;
		}

		u16 value = m_regs.get(m_instruction.rt);
		bus.write<u16>(address, value);
}

void Cpu::SW() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;

		if (address % 4 != 0) {
				m_regs.cop0.bva = address;
				ExceptionHandler(Exception::BadStoreAddress);
				return;
		}

		u32 value = m_regs.get(m_instruction.rt);
		bus.write<u32>(address, value);
}

void Cpu::SWL() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;

		u32 addr_aligned = address & ~3;
		u32 value = bus.read<u32>(addr_aligned);

		u32 rt = m_regs.get(m_instruction.rt);
		u32 final = 0;

		switch (address & 3) {
				case 0: {
						final = (value & 0xffffff00) | (rt >> 24);
						break;
				}
				case 1: {
						final = (value & 0xffff0000) | (rt >> 16);
						break;
				}
				case 2: {
						final = (value & 0xff000000) | (rt >> 8);
						break;
				}
				case 3: {
						final = rt;
						break;
				}
		}
		bus.write<u32>(addr_aligned, final);
}

void Cpu::SWR() {
		u32 address = m_regs.get(m_instruction.rs) + m_instruction.immse;

		u32 addr_aligned = address & ~3;
		u32 value = bus.read<u32>(addr_aligned);
		u32 rt = m_regs.get(m_instruction.rt);
		u32 final = 0;

		switch (address & 3) {
				case 0: {
						final = rt;
						break;
				}
				case 1: {
						final = (value & 0x000000ff) | (rt << 8);
						break;
				}
				case 2: {
						final = (value & 0x0000ffff) | (rt << 16);
						break;
				}
				case 3: {
						final = (value & 0x00ffffff) | (rt << 24);
						break;
				}
		}
		bus.write<u32>(addr_aligned, final);
}

//// ALU

void Cpu::ADD() {
		s32 rs = m_regs.get(m_instruction.rs);
		s32 rt = m_regs.get(m_instruction.rt);
		u32 value = rs + rt;

		bool overflow = ((rs ^ value) & (rt ^ value)) >> 31;
		if (overflow) {
				ExceptionHandler(Exception::Overflow);
				return;
		}

		if (m_instruction.rd != 0) {
				m_regs.set(m_instruction.rd, value);
		}
}

void Cpu::ADDI() {
		s32 rs = m_regs.get(m_instruction.rs);
		s32 imm = m_instruction.immse;
		u32 value = rs + imm;

		bool overflow = ((rs ^ value) & (imm ^ value)) >> 31;
		if (overflow) {
				ExceptionHandler(Exception::Overflow);
				return;
		}

		if (m_instruction.rt != 0) {
				m_regs.set(m_instruction.rt, value);
		}
}

void Cpu::ADDIU() {
		if (!m_instruction.rt) return;
		u32 rs = m_regs.get(m_instruction.rs);
		u32 imm = m_instruction.immse;
		u32 value = rs + imm;
		m_regs.set(m_instruction.rt, value);
}

void Cpu::ADDU() {
		if (!m_instruction.rd) return;
		u32 rs = m_regs.get(m_instruction.rs);
		u32 rt = m_regs.get(m_instruction.rt);
		u32 value = rs + rt;
		m_regs.set(m_instruction.rd, value);
}

void Cpu::SUB() {
		if (!m_instruction.rd) return;
		s32 rs = m_regs.get(m_instruction.rs);
		s32 rt = m_regs.get(m_instruction.rt);

		s32 value = rs - rt;

		bool overflow = ((rs ^ value) & (~rt ^ value)) >> 31;
		if (overflow) {
				ExceptionHandler(Exception::Overflow);
				return;
		}
		if (m_instruction.rd) {
				m_regs.set(m_instruction.rd, value);
		}
}

void Cpu::SUBU() {
		if (!m_instruction.rd) return;

		u32 value = m_regs.get(m_instruction.rs) - m_regs.get(m_instruction.rt);
		m_regs.set(m_instruction.rd, value);
}

void Cpu::OR() {
		if (!m_instruction.rd) return;
		u32 value = m_regs.get(m_instruction.rs) | m_regs.get(m_instruction.rt);
		m_regs.set(m_instruction.rd, value);
}

void Cpu::ORI() {
		if (!m_instruction.rt) return;
		u32 value = m_regs.get(m_instruction.rs) | m_instruction.imm;
		m_regs.set(m_instruction.rt, value);
}

void Cpu::NOR() {
		if (!m_instruction.rd) return;
		u32 rs = m_regs.get(m_instruction.rs);
		u32 rt = m_regs.get(m_instruction.rt);

		u32 value = ~(rs | rt);
		m_regs.set(m_instruction.rd, value);
}

void Cpu::XOR() {
		//    panic("TEMPORARY TEST\n");
		if (!m_instruction.rd) return;
		u32 value = m_regs.get(m_instruction.rs) ^ m_regs.get(m_instruction.rt);
		m_regs.set(m_instruction.rd, value);
}

void Cpu::XORI() {
		if (!m_instruction.rt) return;
		u32 value = m_regs.get(m_instruction.rs) ^ m_instruction.imm;
		m_regs.set(m_instruction.rt, value);
}

void Cpu::SLL() {
		if (!m_instruction.rd) return;

		m_regs.set(m_instruction.rd, m_regs.get(m_instruction.rt) << m_instruction.sa);
}

void Cpu::SRA() {
		if (!m_instruction.rd) return;
		s32 rt = m_regs.get(m_instruction.rt);
		u32 value = rt >> m_instruction.sa;
		m_regs.set(m_instruction.rd, value);
}

void Cpu::SRAV() {
		if (!m_instruction.rd) return;
		s32 rt = m_regs.get(m_instruction.rt);
		u32 rs = m_regs.get(m_instruction.rs);

		u32 value = rt >> (rs & 0x1f);
		m_regs.set(m_instruction.rd, value);
}

void Cpu::SRLV() {
		if (!m_instruction.rd) return;
		u32 rt = m_regs.get(m_instruction.rt);
		u32 rs = m_regs.get(m_instruction.rs);

		u32 value = rt >> (rs & 0x1f);
		m_regs.set(m_instruction.rd, value);
}

void Cpu::SRL() {
		if (!m_instruction.rd) return;
		u32 value = m_regs.get(m_instruction.rt) >> m_instruction.sa;
		m_regs.set(m_instruction.rd, value);
}

void Cpu::AND() {
		u32 value = m_regs.get(m_instruction.rs) & m_regs.get(m_instruction.rt);
		m_regs.set(m_instruction.rd, value);
}

void Cpu::ANDI() {
		u32 value = m_regs.get(m_instruction.rs) & m_instruction.imm;
		m_regs.set(m_instruction.rt, value);
}

// divu $s, $t | lo = $s / $t; hi = $s % $t
// dividend ÷ divisor = quotient
void Cpu::DIV() {
		s32 dividend = m_regs.get(m_instruction.rs);
		s32 divisor = m_regs.get(m_instruction.rt);

		if (divisor == 0) {
				m_regs.hi = (u32)dividend;

				if (dividend >= 0) {
						m_regs.lo = 0xffffffff;
				} else {
						m_regs.lo = 1;
				}
				return;
		}

		if ((u32)dividend == 0x80000000 && divisor == -1) {
				m_regs.hi = 0;
				m_regs.lo = 0x80000000;
				return;
		}

		m_regs.hi = static_cast<u32>(dividend % divisor);
		m_regs.lo = static_cast<u32>(dividend / divisor);
}

void Cpu::DIVU() {
		u32 dividend = m_regs.get(m_instruction.rs);
		u32 divisor = m_regs.get(m_instruction.rt);

		if (divisor == 0) {
				m_regs.hi = dividend;
				m_regs.lo = 0xffffffff;
				return;
		}

		m_regs.hi = dividend % divisor;
		m_regs.lo = dividend / divisor;
}

void Cpu::MULT() {
		auto rs = (s64)(s32)m_regs.get(m_instruction.rs);
		auto rt = (s64)(s32)m_regs.get(m_instruction.rt);

		s64 value = rs * rt;

		m_regs.hi = (u32)((value >> 32) & 0xffffffff);
		m_regs.lo = (u32)(value & 0xffffffff);
}

void Cpu::MULTU() {
		u64 rs = m_regs.get(m_instruction.rs);
		u64 rt = m_regs.get(m_instruction.rt);

		u64 value = rs * rt;

		m_regs.hi = static_cast<u32>(value >> 32);
		m_regs.lo = (u32)value;
}

void Cpu::MFLO() {
		if (!m_instruction.rd) return;
		m_regs.set(m_instruction.rd, m_regs.lo);
}

void Cpu::MFHI() {
		if (!m_instruction.rd) return;
		m_regs.set(m_instruction.rd, m_regs.hi);
}

void Cpu::MTLO() { m_regs.lo = m_regs.get(m_instruction.rs); }

void Cpu::MTHI() { m_regs.hi = m_regs.get(m_instruction.rs); }

//// CONDITONAL/BRANCH
//
void Cpu::J() {
		m_branch = true;
		m_branchTaken = true;
		m_next_pc = (m_next_pc & 0xf0000000) | (m_instruction.tar << 2);
}

void Cpu::JAL() {
		m_regs.set(31, m_next_pc);
		J();
}

void Cpu::JR() {
		m_branch = true;
		m_branchTaken = true;
		m_next_pc = m_regs.get(m_instruction.rs);
}

void Cpu::JALR() {
		// TODO Maybe skip if reg index rs == rd to prevent register clobbering
		m_regs.set(m_instruction.rd, m_next_pc);
		JR();
}

void Cpu::BEQ() {
		m_branch = true;
		u32 rs = m_regs.get(m_instruction.rs);
		u32 rt = m_regs.get(m_instruction.rt);

		if (rs == rt) {
				Branch();
		}
}

void Cpu::BNE() {
		m_branch = true;
		u32 rs = m_regs.get(m_instruction.rs);
		u32 rt = m_regs.get(m_instruction.rt);

		if (rs != rt) {
				Branch();
		}
}

void Cpu::BGTZ() {
		m_branch = true;
		s32 rs = m_regs.get(m_instruction.rs);
		if (rs > 0) {
				Branch();
		}
}

void Cpu::BLEZ() {
		m_branch = true;
		s32 rs = m_regs.get(m_instruction.rs);
		if (rs <= 0) {
				Branch();
		}
}

void Cpu::Branch(bool link) {
		m_branchTaken = true;
		m_next_pc = m_pc + m_instruction.immse * 4;
		if (link) {
				m_regs.set(31, m_next_pc);
		}
}

void Cpu::REGIMM() {
		m_branch = true;
		s32 rs = m_regs.get(m_instruction.rs);
		if (m_instruction.bgez) {
				if (rs >= 0) {
						Branch();
				}
		} else {
				if (rs < 0) {
						Branch();
				}
		}
		// Link should happen regardless if branch taken
		if ((m_instruction.rt & 0x1E) == 0x10) {
				m_regs.set(31, m_next_pc);
		}
}

void Cpu::SLTU() {
		if (!m_instruction.rd) return;

		u32 value = m_regs.get(m_instruction.rs) < m_regs.get(m_instruction.rt);
		m_regs.set(m_instruction.rd, value);
}

void Cpu::SLTI() {
		if (!m_instruction.rt) return;

		s32 rs = m_regs.get(m_instruction.rs);
		s32 imm = m_instruction.immse;
		u32 value = rs < imm;
		m_regs.set(m_instruction.rt, value);
}

void Cpu::SLTIU() {
		if (!m_instruction.rt) return;

		u32 rs = m_regs.get(m_instruction.rs);
		u32 value = rs < m_instruction.immse;
		m_regs.set(m_instruction.rt, value);
}

void Cpu::SLT() {
		if (!m_instruction.rd) return;

		s32 rs = m_regs.get(m_instruction.rs);
		s32 rt = m_regs.get(m_instruction.rt);
		s32 value = rs < rt;

		m_regs.set(m_instruction.rd, static_cast<u32>(value));
}

void Cpu::SLLV() {
		if (!m_instruction.rd) return;

		u32 rs = m_regs.get(m_instruction.rs);
		u32 rt = m_regs.get(m_instruction.rt);
		u32 value = rt << (rs & 0x1f);

		m_regs.set(m_instruction.rd, value);
}

// COP

void Cpu::COP0() {
		switch (m_instruction.rs) {
				case 0:
						MFC0();
						break;
				case 4:
						MTC0();
						break;
				case 16:
						RFE();
						break;
				default:
						Log::warn("Unimplemented COP0 instruction {:#x}\n", m_instruction.opcode);
		}
}

void Cpu::MTC0() {
		u32 val = m_regs.get(m_instruction.rt);
		u32 rd = m_instruction.rd;

		bool prev = (m_regs.cop0.sr & 0x1) == 1;

		if (rd == 13) {
				m_regs.cop0.cause &= ~(u32)0x300;
				m_regs.cop0.cause |= val & 0x300;
		} else {
				m_regs.setcop(rd, val);
		}

		u32 imask = (val >> 8) & 0x3;
		u32 ipending = (m_regs.cop0.cause >> 8) & 0x3;

		if (!prev && (m_regs.cop0.sr & 0x1) == 1 && (imask & ipending) > 0) {
				m_pc = m_next_pc;
				ExceptionHandler(Exception::Interrupt);
		}
}

void Cpu::MFC0() {
		u32 rd = m_instruction.rd;

		m_regs.pendingLoad(m_instruction.rt, m_regs.getcop(rd));

		if (rd == 3 || rd >= 5 && rd <= 9 || rd >= 11 && rd <= 15) {
		} else {
				ExceptionHandler(Exception::IllegalInstruction);
		}
}

// Unimplemented Instructions

void Cpu::Unknown() { Helpers::panic("Unknown instruction at {:#x}, opcode {:#x}\n", m_pc, m_instruction.code); }

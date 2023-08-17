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

void Cpu::run() {}

void Cpu::step() {
    // Fetch
    if (PC == SHELL_PC) {
        bus.shellReached();
    }
    instruction = bus.fetch(PC);

    if ((PC % 4) != 0) {
        Log::warn("[CPU] Unaligned PC {:#x}\n", PC);
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
    const auto func = basic[instruction.opcode];
    (this->*func)();

    // Do memory access
    if (delayedLoad.reg != memoryLoad.reg) {
        regs.gpr[memoryLoad.reg] = memoryLoad.value;
    }

    memoryLoad = delayedLoad;
    delayedLoad.reset();

    // Write back registers
    regs.writeback();

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
                Log::info("{}", c);
                break;
            }
            case 0x08: {
                const u32 e = regs.gpr[A0];
                Log::info("[OpenEvent] {} - {:#08X}\n", magic_enum::enum_name<KernelEvents>(static_cast<KernelEvents>(e & 0xFFFFFFF)), e);
                break;
            }
        }
    }
}

void Cpu::triggerInterrupt() {}

void Cpu::checkInterrupts() {
    if (bus.isIRQPending()) {
        regs.cop0.cause |= static_cast<u32>(0x400);
    } else {
        regs.cop0.cause &= static_cast<u32>(~0x400);
    }

    bool iec = (regs.cop0.status & 1) == 1;
    u8 im = (regs.cop0.status >> 8) & 0xFF;
    u8 ip = (regs.cop0.cause >> 8) & 0xFF;

    if (iec && (im & ip) > 0) {
        ExceptionHandler(Exception::Interrupt);
    }
}

void Cpu::Unknown() { Helpers::panic("[CPU] Unknown instruction at {:#x}, opcode {:#x}\n", PC, instruction.code); }

void Cpu::NOP() {}

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
        if (branchTakenDelaySlot) {
            regs.cop0.cause |= (1 << 30);
        }
    } else {
        regs.cop0.cause &= ~(1 << 31);
    }

    setPC(vector);
    //    Log::debug("ExceptionHandler at PC {:#08x}\n", currentPC);
}

void Cpu::RFE() {
    if ((instruction.code & 0x3f) != 0x10) {
        Log::warn("[CPU] RFE: Unmatched cop0 instruction\n");
        return;
    }
    u32 mode = regs.cop0.status & 0x3F;
    regs.cop0.status &= ~(u32)0xF;
    regs.cop0.status |= mode >> 2;
    //    Log::debug("Return from Exception\n");
}

void Cpu::SYSCALL() { ExceptionHandler(Exception::Syscall); }

void Cpu::BREAK() { ExceptionHandler(Exception::Break); }

void Cpu::Special() {
    const auto f = special[instruction.fn];
    (this->*f)();
}

// Branch and Jump Instructions
void Cpu::Branch(bool link) {
    branchTaken = true;
    nextPC = PC + (instruction.immse * 4);
    if (link) {
        regs.set(RA, nextPC);
    }
}

void Cpu::REGIMM() {
    branch = true;
    s32 rs = regs.get(instruction.rs);
    if (instruction.bgez) {
        if (rs >= 0) {
            Branch();
        }
    } else {
        if (rs < 0) {
            Branch();
        }
    }
    // Link should happen regardless if branch taken
    if ((instruction.rt & 0x1E) == 0x10) {
        regs.set(RA, nextPC);
    }
}

void Cpu::J() {
    branch = true;
    branchTaken = true;
    nextPC = (nextPC & 0xf0000000) | (instruction.tar << 2);
}

void Cpu::JAL() {
    regs.set(RA, nextPC);
    J();
}

void Cpu::JALR() {
    regs.set(instruction.rd, nextPC);
    nextPC = regs.get(instruction.rs);
    branch = true;
    branchTaken = true;
}

void Cpu::JR() {
    branch = true;
    branchTaken = true;
    nextPC = regs.get(instruction.rs);
}

void Cpu::BEQ() {
    branch = true;
    u32 rs = regs.get(instruction.rs);
    u32 rt = regs.get(instruction.rt);

    if (rs == rt) {
        Branch();
    }
}

void Cpu::BNE() {
    branch = true;
    u32 rs = regs.get(instruction.rs);
    u32 rt = regs.get(instruction.rt);

    if (rs != rt) {
        Branch();
    }
}

void Cpu::BGTZ() {
    branch = true;
    s32 rs = regs.get(instruction.rs);
    if (rs > 0) {
        Branch();
    }
}

void Cpu::BLEZ() {
    branch = true;
    s32 rs = regs.get(instruction.rs);
    if (rs <= 0) {
        Branch();
    }
}

// ALU Instructions

void Cpu::ADD() {
    s32 rs = static_cast<s32>(regs.get(instruction.rs));
    s32 rt = static_cast<s32>(regs.get(instruction.rt));
    u32 value = static_cast<u32>(rs + rt);

    bool overflow = ((rs ^ value) & (rt ^ value)) >> 31;
    if (overflow) {
        ExceptionHandler(Exception::Overflow);
        return;
    }

    if (instruction.rd != 0) {
        regs.set(instruction.rd, value);
    }
}

void Cpu::ADDI() {
    s32 rs = static_cast<s32>(regs.get(instruction.rs));
    s32 imm = instruction.immse;
    u32 value = static_cast<u32>(rs + imm);

    bool overflow = ((rs ^ value) & (imm ^ value)) >> 31;
    if (overflow) {
        ExceptionHandler(Exception::Overflow);
        return;
    }

    if (instruction.rt != 0) {
        regs.set(instruction.rt, value);
    }
}

void Cpu::ADDIU() {
    if (!instruction.rt) return;
    u32 rs = regs.get(instruction.rs);
    u32 imm = instruction.immse;
    u32 value = rs + imm;
    regs.set(instruction.rt, value);
}

void Cpu::ADDU() {
    if (!instruction.rd) return;
    u32 rs = regs.get(instruction.rs);
    u32 rt = regs.get(instruction.rt);
    u32 value = rs + rt;
    regs.set(instruction.rd, value);
}

void Cpu::AND() {
    u32 value = regs.get(instruction.rs) & regs.get(instruction.rt);
    regs.set(instruction.rd, value);
}

void Cpu::ANDI() {
    u32 value = regs.get(instruction.rs) & instruction.imm;
    regs.set(instruction.rt, value);
}

void Cpu::OR() {
    if (!instruction.rd) return;
    u32 value = regs.get(instruction.rs) | regs.get(instruction.rt);
    regs.set(instruction.rd, value);
}

void Cpu::ORI() {
    if (!instruction.rt) return;
    u32 value = regs.get(instruction.rs) | instruction.imm;
    regs.set(instruction.rt, value);
}

void Cpu::XOR() {
    if (!instruction.rd) return;
    u32 value = regs.get(instruction.rs) ^ regs.get(instruction.rt);
    regs.set(instruction.rd, value);
}

void Cpu::XORI() {
    if (!instruction.rt) return;
    u32 value = regs.get(instruction.rs) ^ instruction.imm;
    regs.set(instruction.rt, value);
}

void Cpu::NOR() {
    if (!instruction.rd) return;
    u32 rs = regs.get(instruction.rs);
    u32 rt = regs.get(instruction.rt);

    u32 value = ~(rs | rt);
    regs.set(instruction.rd, value);
}

void Cpu::SLTU() {
    if (!instruction.rd) return;

    u32 value = regs.get(instruction.rs) < regs.get(instruction.rt);
    regs.set(instruction.rd, value);
}

void Cpu::SLTI() {
    if (!instruction.rt) return;

    s32 rs = regs.get(instruction.rs);
    s32 imm = instruction.immse;
    u32 value = rs < imm;
    regs.set(instruction.rt, value);
}

void Cpu::SLTIU() {
    if (!instruction.rt) return;

    u32 rs = regs.get(instruction.rs);
    u32 value = rs < instruction.immse;
    regs.set(instruction.rt, value);
}

void Cpu::SLT() {
    if (!instruction.rd) return;

    s32 rs = regs.get(instruction.rs);
    s32 rt = regs.get(instruction.rt);
    s32 value = rs < rt;

    regs.set(instruction.rd, static_cast<u32>(value));
}

void Cpu::SLLV() {
    if (!instruction.rd) return;

    u32 rs = regs.get(instruction.rs);
    u32 rt = regs.get(instruction.rt);
    u32 value = rt << (rs & 0x1f);

    regs.set(instruction.rd, value);
}

void Cpu::SRA() {
    if (!instruction.rd) return;
    s32 rt = regs.get(instruction.rt);
    u32 value = rt >> instruction.sa;
    regs.set(instruction.rd, value);
}

void Cpu::SRAV() {
    if (!instruction.rd) return;
    s32 rt = regs.get(instruction.rt);
    u32 rs = regs.get(instruction.rs);

    u32 value = rt >> (rs & 0x1f);
    regs.set(instruction.rd, value);
}

void Cpu::SRL() {
    if (!instruction.rd) return;
    u32 value = regs.get(instruction.rt) >> instruction.sa;
    regs.set(instruction.rd, value);
}

void Cpu::SRLV() {
    if (!instruction.rd) return;
    u32 rt = regs.get(instruction.rt);
    u32 rs = regs.get(instruction.rs);

    u32 value = rt >> (rs & 0x1f);
    regs.set(instruction.rd, value);
}

void Cpu::SUB() {
    if (!instruction.rd) return;
    s32 rs = static_cast<s32>(regs.get(instruction.rs));
    s32 rt = static_cast<s32>(regs.get(instruction.rt));

    s32 value = rs - rt;

    bool overflow = ((rs ^ value) & (~rt ^ value)) >> 31;
    if (overflow) {
        ExceptionHandler(Exception::Overflow);
        return;
    }
    if (instruction.rd) {
        regs.set(instruction.rd, value);
    }
}

void Cpu::SUBU() {
    if (!instruction.rd) return;

    u32 value = regs.get(instruction.rs) - regs.get(instruction.rt);
    regs.set(instruction.rd, value);
}

void Cpu::SLL() {
    if (!instruction.rd) return;

    regs.set(instruction.rd, regs.get(instruction.rt) << instruction.sa);
}

void Cpu::DIV() {
    s32 dividend = regs.get(instruction.rs);
    s32 divisor = regs.get(instruction.rt);

    if (divisor == 0) {
        regs.gpr[HI] = (u32)dividend;

        if (dividend >= 0) {
            regs.gpr[LO] = 0xffffffff;
        } else {
            regs.gpr[LO] = 1;
        }
        return;
    }

    if ((u32)dividend == 0x80000000 && divisor == -1) {
        regs.gpr[HI] = 0;
        regs.gpr[LO] = 0x80000000;
        return;
    }

    regs.gpr[HI] = static_cast<u32>(dividend % divisor);
    regs.gpr[LO] = static_cast<u32>(dividend / divisor);
}

void Cpu::DIVU() {
    u32 dividend = regs.get(instruction.rs);
    u32 divisor = regs.get(instruction.rt);

    if (divisor == 0) {
        regs.gpr[HI] = dividend;
        regs.gpr[LO] = 0xffffffff;
        return;
    }

    regs.gpr[HI] = dividend % divisor;
    regs.gpr[LO] = dividend / divisor;
}

void Cpu::MTHI() { regs.gpr[HI] = regs.get(instruction.rs); }

void Cpu::MTLO() { regs.gpr[LO] = regs.get(instruction.rs); }

void Cpu::MFHI() {
    if (!instruction.rd) return;
    regs.set(instruction.rd, regs.gpr[HI]);
}

void Cpu::MFLO() {
    if (!instruction.rd) return;
    regs.set(instruction.rd, regs.gpr[LO]);
}

void Cpu::MULT() {
    auto rs = (s64)(s32)regs.get(instruction.rs);
    auto rt = (s64)(s32)regs.get(instruction.rt);

    s64 value = rs * rt;

    regs.gpr[HI] = (u32)((value >> 32) & 0xffffffff);
    regs.gpr[LO] = (u32)(value & 0xffffffff);
}

void Cpu::MULTU() {
    u64 rs = regs.get(instruction.rs);
    u64 rt = regs.get(instruction.rt);

    u64 value = rs * rt;

    regs.gpr[HI] = static_cast<u32>(value >> 32);
    regs.gpr[LO] = (u32)value;
}

// Cop0 Instructions

void Cpu::COP0() {
    switch (instruction.rs) {
        case 0: MFC0(); break;
        case 4: MTC0(); break;
        case 16: RFE(); break;
        default: Log::warn("[CPU] Unimplemented COP0 instruction {:#x}\n", instruction.opcode);
    }
}

void Cpu::MFC0() {
    u32 rd = instruction.rd;
    u32 val = 0;

    switch (rd) {
        case SR: val = regs.cop0.status; break;
        case CAUSE: val = regs.cop0.cause; break;
        case BVA: val = regs.cop0.bva; break;
        case EPC: val = regs.cop0.epc; break;
        case PRID: val = 1; break;
        default: Log::warn("[CPU] Unhandled COP0 register read: {}\n", rd);
    }
    delayedLoad.set(instruction.rt, val);
}

void Cpu::MTC0() {
    u32 val = regs.get(instruction.rt);
    u32 rd = instruction.rd;

    bool prev = (regs.cop0.status & 0x1) == 1;

    if (rd == 13) {
        regs.cop0.cause &= ~(u32)0x300;
        regs.cop0.cause |= val & 0x300;
    } else if (rd == 12) {
        regs.cop0.status = val;
    }

    u32 imask = (val >> 8) & 0x3;
    u32 ipending = (regs.cop0.cause >> 8) & 0x3;

    if (!prev && (regs.cop0.status & 0x1) == 1 && (imask & ipending) > 0) {
        PC = nextPC;
        ExceptionHandler(Exception::Interrupt);
    }
}

// Load/Store Instructions
void Cpu::LUI() { regs.set(instruction.rt, instruction.imm << 16); }

void Cpu::LB() {
    u32 addr = regs.get(instruction.rs) + instruction.immse;
    delayedLoad.set(instruction.rt, Helpers::signExtend<u32, u8>(bus.read<u8>(addr)));
}

void Cpu::LBU() {
    u32 address = regs.get(instruction.rs) + instruction.immse;
    delayedLoad.set(instruction.rt, bus.read<u8>(address));
}

void Cpu::LH() {
    u32 address = regs.get(instruction.rs) + instruction.immse;
    if (address % 2 != 0) {
        regs.cop0.bva = address;
        ExceptionHandler(Exception::BadLoadAddress);
        return;
    }
    delayedLoad.set(instruction.rt, Helpers::signExtend<u32, u16>(bus.read<u16>(address)));
}

void Cpu::LHU() {
    u32 address = regs.get(instruction.rs) + instruction.immse;
    if (address % 2 != 0) {
        regs.cop0.bva = address;
        ExceptionHandler(Exception::BadLoadAddress);
        return;
    }
    delayedLoad.set(instruction.rt, bus.read<u16>(address));
}

void Cpu::LW() {
    u32 address = regs.get(instruction.rs) + instruction.immse;

    if (address % 4 != 0) {
        regs.cop0.bva = address;
        ExceptionHandler(Exception::BadLoadAddress);
        return;
    }
    delayedLoad.set(instruction.rt, bus.read<u32>(address));
}

void Cpu::LWL() {
    u32 address = regs.get(instruction.rs) + instruction.immse;

    u32 addr_aligned = address & ~3;
    u32 value = bus.read<u32>(addr_aligned);

    u32 final = 0;

    u32 pending = regs.get(instruction.rt);

    if (instruction.rt == memoryLoad.reg) {
        pending = memoryLoad.value;
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
        case 3: final = value; break;
    }
    delayedLoad.set(instruction.rt, final);
}

void Cpu::LWR() {
    u32 address = regs.get(instruction.rs) + instruction.immse;

    u32 addr_aligned = address & 0xFFFFFFFC;
    u32 value = bus.read<u32>(addr_aligned);

    u32 final = 0;

    u32 pending = regs.get(instruction.rt);
    if (instruction.rt == memoryLoad.reg) {
        pending = memoryLoad.value;
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

    delayedLoad.set(instruction.rt, final);
}

void Cpu::SB() {
    u32 address = regs.get(instruction.rs) + instruction.immse;
    u8 value = regs.get(instruction.rt);
    bus.write<u8>(address, value);
}

void Cpu::SH() {
    u32 address = regs.get(instruction.rs) + instruction.immse;

    if (address % 2 != 0) {
        regs.cop0.bva = address;
        ExceptionHandler(Exception::BadStoreAddress);
        return;
    }

    u16 value = regs.get(instruction.rt);
    bus.write<u16>(address, value);
}

void Cpu::SW() {
    u32 address = regs.get(instruction.rs) + instruction.immse;

    if (address % 4 != 0) {
        regs.cop0.bva = address;
        ExceptionHandler(Exception::BadStoreAddress);
        return;
    }

    u32 value = regs.get(instruction.rt);
    bus.write<u32>(address, value);
}

void Cpu::SWL() {
    u32 address = regs.get(instruction.rs) + instruction.immse;

    u32 addr_aligned = address & ~3;
    u32 value = bus.read<u32>(addr_aligned);

    u32 rt = regs.get(instruction.rt);
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
    u32 address = regs.get(instruction.rs) + instruction.immse;

    u32 addr_aligned = address & ~3;
    u32 value = bus.read<u32>(addr_aligned);
    u32 rt = regs.get(instruction.rt);
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

// GTE Instructions
void Cpu::SWC2() {}
void Cpu::LWC2() {}
void Cpu::CFC2() {}
void Cpu::CTC2() {}
void Cpu::MFC2() {}
void Cpu::MTC2() {}
void Cpu::COP2() {}
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

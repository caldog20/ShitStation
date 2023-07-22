#include "cpu.hpp"
#include "support/utils.hpp"

void Cpu::COP2() {
		//    m_log->debug("GTE Instructions");
		const auto gtefunc = gte[m_instruction.opcode & 0x3f];
		(this->*gtefunc)();
}

void Cpu::GTEMove() {
		switch (m_instruction.rs) {
				case 0:
						MFC2();
						break;
				case 2:
						CFC2();
						break;
				case 4:
						MTC2();
						break;
				case 6:
						CTC2();
						break;
				default:
						Unknown();
		}
}

void Cpu::CFC2() {}
void Cpu::CTC2() {}
void Cpu::LWC2() {}
void Cpu::MFC2() {}
void Cpu::MTC2() {}
void Cpu::SWC2() {}

// GTE Instructions

void Cpu::AVSZ3() { /* m_log->debug("[Unimplemented] AVSZ3 instruction"); */
}
void Cpu::AVSZ4() { /* m_log->debug("[Unimplemented] AVSZ4 instruction"); */
}
void Cpu::CC() { /* m_log->debug("[Unimplemented] CC instruction"); */
}
void Cpu::CDP() { /* m_log->debug("[Unimplemented] CDP instruction"); */
}
void Cpu::DCPL() { /* m_log->debug("[Unimplemented] DCPL instruction"); */
}
void Cpu::DPCS() { /* m_log->debug("[Unimplemented] DPCS instruction"); */
}
void Cpu::DPCT() { /* m_log->debug("[Unimplemented] DPCT instruction"); */
}
void Cpu::GPF() { /* m_log->debug("[Unimplemented] GPF instruction"); */
}
void Cpu::GPL() { /* m_log->debug("[Unimplemented] GPL instruction"); */
}
void Cpu::INTPL() { /* m_log->debug("[Unimplemented] INTPL instruction"); */
}
void Cpu::MVMVA() { /* m_log->debug("[Unimplemented] MVMVA instruction"); */
}
void Cpu::NCCS() { /* m_log->debug("[Unimplemented] NCCS instruction"); */
}
void Cpu::NCCT() { /* m_log->debug("[Unimplemented] NCCT instruction"); */
}
void Cpu::NCDS() { /* m_log->debug("[Unimplemented] NCDS instruction"); */
}
void Cpu::NCDT() { /* m_log->debug("[Unimplemented] NCDT instruction"); */
}
void Cpu::NCLIP() { /* m_log->debug("[Unimplemented] NCLIP instruction"); */
}
void Cpu::NCS() { /* m_log->debug("[Unimplemented] NCS instruction"); */
}
void Cpu::NCT() { /* m_log->debug("[Unimplemented] NCT instruction"); */
}
void Cpu::OP() { /* m_log->debug("[Unimplemented] OP instruction"); */
}
void Cpu::RTPS() { /* m_log->debug("[Unimplemented] RTPS instruction"); */
}
void Cpu::RTPT() { /* m_log->debug("[Unimplemented] RTPT instruction"); */
}
void Cpu::SQR() { /* m_log->debug("[Unimplemented] SQR instruction"); */
}

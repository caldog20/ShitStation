#include "spu.hpp"

#include <cassert>

namespace Spu {

Spu::Spu() {}

Spu::~Spu() {}

void Spu::reset() {
    spuram.clear();
    spuram.resize(512_KB);
    std::memset(&control, 0, sizeof(control));
    std::memset(voices, 0, sizeof(voices));
    currentAddress = 0;
}

u8 Spu::read8(u32 address) { return 0; }

u16 Spu::read16(u32 address) {
    auto voice = (address - 0x1F801C00) >> 4;
    if (address >= 0x1F801C00 && address < 0x1F801D80) {
        auto& v = voices[voice];

        switch (address & 0xF) {
            case 0x0: return v.VolumeLeft;
            case 0x2: return v.VolumeRight;
            case 0x4: return v.ADPCMSampleRate;
            case 0x6: return v.ADPCMStartAddress;
            case 0x8: return v.AD;
            case 0xA: return v.SR;
            case 0xC: return v.ASDRVolume;
            case 0xE: return v.ADPCMRepeatAddress;
            default: Log::warn("[SPU] unhandled read16 Voice Registers {:#x}\n", address); break;
        }
    }

    // Control Registers
    switch (address) {
        case 0x1F801D80: return control.MainVolumeLeft;
        case 0x1F801D82: return control.MainVolumeRight;
        case 0x1F801D84: return control.ReverbOutputVolumeLeft;
        case 0x1F801D86: return control.ReverbOutputVolumeRight;
        // KONreturn
        case 0x1F801D88: return getVoiceRegister<VoiceRegister::KON, true>();
        case 0x1F801D8A: return getVoiceRegister<VoiceRegister::KON, false>();
        // KOFFreturn
        case 0x1F801D8C: return getVoiceRegister<VoiceRegister::KOFF, true>();
        case 0x1F801D8E: return getVoiceRegister<VoiceRegister::KOFF, false>();
        // Channel FMreturn
        case 0x1F801D90: return getVoiceRegister<VoiceRegister::FM, true>();
        case 0x1F801D92: return getVoiceRegister<VoiceRegister::FM, false>();
        // Channel Noisereturn
        case 0x1F801D94: return getVoiceRegister<VoiceRegister::NON, true>();
        case 0x1F801D96: return getVoiceRegister<VoiceRegister::NON, false>();
        // Channel Reverbreturn
        case 0x1F801D98: return getVoiceRegister<VoiceRegister::Reverb, true>();
        case 0x1F801D9A: return getVoiceRegister<VoiceRegister::Reverb, false>();
        // Channel Statusreturn
        case 0x1F801D9C: return getVoiceRegister<VoiceRegister::Status, true>();
        case 0x1F801D9E: return getVoiceRegister<VoiceRegister::Status, false>();
        case 0x1F801DA2: return control.RWASA;
        case 0x1F801DA4: return control.IRQAddress;
        case 0x1F801DA6: return control.DataTransferAddress;
        case 0x1F801DA8: return control.DataTransferFifo;
        case 0x1F801DAA: return control.SPUCNT.r;
        case 0x1F801DAC: return control.DataTransferControl;
        case 0x1F801DB0: return control.CDVolumeLeft;
        case 0x1F801DB2: return control.CDVolumeRight;
        case 0x1F801DB4: return control.ExternVolumeLeft;
        case 0x1F801DB6: return control.ExternVolumeRight;
        case 0x1F801DB8: return control.CurrentVolumeLeft;
        case 0x1F801DBA: return control.CurrentVolumeRight;
        case 0x1F801DAE: return control.SPUSTAT.r;
        default: Log::warn("[SPU] unhandled read16 {:#x}\n", address); break;
    }
}

u32 Spu::read32(u32 address) { return 0; }

void Spu::write8(u32 address, u8 value) { Log::debug("[SPU] write8 {:#x} {:#x}\n", address, value); }

void Spu::write16(u32 address, u16 value) {
    //    Log::debug("[SPU] write16 {:#x} {:#x}\n", address, value);
    // Voice Registers
    auto voice = (address - 0x1F801C00) >> 4;
    if (address >= 0x1F801C00 && address < 0x1F801D80) {
        auto& v = voices[voice];

        switch (address & 0xF) {
            case 0x0: v.VolumeLeft = value; break;
            case 0x2: v.VolumeRight = value; break;
            case 0x4: v.ADPCMSampleRate = value; break;
            case 0x6: v.ADPCMStartAddress = value; break;
            case 0x8: v.AD = value; break;
            case 0xA: v.SR = value; break;
            case 0xC: v.ASDRVolume = value; break;
            case 0xE: v.ADPCMRepeatAddress = value; break;
        }
        return;
    }

    // Control Registers
    switch (address) {
        case 0x1F801D80: control.MainVolumeLeft = value; break;
        case 0x1F801D82: control.MainVolumeRight = value; break;
        case 0x1F801D84: control.ReverbOutputVolumeLeft = value; break;
        case 0x1F801D86: control.ReverbOutputVolumeRight = value; break;
        // KON
        case 0x1F801D88: setVoiceRegister<VoiceRegister::KON, true>(value); break;
        case 0x1F801D8A: setVoiceRegister<VoiceRegister::KON, false>(value); break;
        // KOFF
        case 0x1F801D8C: setVoiceRegister<VoiceRegister::KOFF, true>(value); break;
        case 0x1F801D8E: setVoiceRegister<VoiceRegister::KOFF, false>(value); break;
        // Channel FM
        case 0x1F801D90: setVoiceRegister<VoiceRegister::FM, true>(value); break;
        case 0x1F801D92: setVoiceRegister<VoiceRegister::FM, false>(value); break;
        // Channel Noise
        case 0x1F801D94: setVoiceRegister<VoiceRegister::NON, true>(value); break;
        case 0x1F801D96: setVoiceRegister<VoiceRegister::NON, false>(value); break;
        // Channel Reverb
        case 0x1F801D98: setVoiceRegister<VoiceRegister::Reverb, true>(value); break;
        case 0x1F801D9A: setVoiceRegister<VoiceRegister::Reverb, false>(value); break;
        // Channel Status
        case 0x1F801D9C: setVoiceRegister<VoiceRegister::Status, true>(value); break;
        case 0x1F801D9E: setVoiceRegister<VoiceRegister::Status, false>(value); break;

        case 0x1F801DA2: control.RWASA = value; break;
        case 0x1F801DA4: control.IRQAddress = value; break;
        case 0x1F801DA6:
            control.DataTransferAddress = value;
            currentAddress = value * 8;
            break;
        case 0x1F801DA8: pushFifo(value); break;
        case 0x1F801DAA: control.SPUCNT.r = value; break;
        case 0x1F801DAC: control.DataTransferControl = value; break;
        case 0x1F801DB0: control.CDVolumeLeft = value; break;
        case 0x1F801DB2: control.CDVolumeRight = value; break;
        case 0x1F801DB4: control.ExternVolumeLeft = value; break;
        case 0x1F801DB6: control.ExternVolumeRight = value; break;
        case 0x1F801DB8: control.CurrentVolumeLeft = value; break;
        case 0x1F801DBA: control.CurrentVolumeRight = value; break;
        default: Log::warn("[SPU] unhandled write16 {:#x} {:#x}\n", address, value); break;
    }
}

void Spu::write32(u32 address, u32 value) { Log::debug("[SPU] write32 {:#x} {:#x}\n", address, value); }

void Spu::pushFifo(u16 value) {
    assert(currentAddress < 512_KB);

    spuram[currentAddress] = value & 0xFF;
    spuram[currentAddress + 1] = value >> 8;

    currentAddress += 2;
}

u16 Spu::readRAM() {
    assert(currentAddress < 512_KB);
    u16 value = spuram[currentAddress] | (spuram[currentAddress + 1] << 8);
    currentAddress += 2;
    return value;
}

template <Spu::VoiceRegister reg, bool first>
void Spu::setVoiceRegister(u16 value) {
    using enum VoiceRegister;

    if constexpr (reg == KOFF) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                if (value & (1 << i)) {
                    voices[i].KOFF = true;
                }
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    voices[i + 16].KOFF = true;
                }
            }
        }
    } else if constexpr (reg == KON) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                if (value & (1 << i)) {
                    voices[i].KON = true;
                }
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    voices[i + 16].KON = true;
                }
            }
        }
    } else if constexpr (reg == NON) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                if (value & (1 << i)) {
                    voices[i].NON = true;
                }
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    voices[i + 16].NON = true;
                }
            }
        }
    } else if constexpr (reg == FM) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                if (value & (1 << i)) {
                    voices[i].FM = true;
                }
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    voices[i + 16].FM = true;
                }
            }
        }
    } else if constexpr (reg == Reverb) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                if (value & (1 << i)) {
                    voices[i].Reverb = true;
                }
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    voices[i + 16].Reverb = true;
                }
            }
        }
    } else if constexpr (reg == Status) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                if (value & (1 << i)) {
                    voices[i].Status = true;
                }
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    voices[i + 16].Status = true;
                }
            }
        }
    }
}

template <Spu::VoiceRegister reg, bool first>
u16 Spu::getVoiceRegister() {
    using enum VoiceRegister;
    u16 value = 0;
    if constexpr (reg == KOFF) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                value |= voices[i].KOFF << i;
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                value |= voices[i + 16].KOFF << i;
            }
        }
    } else if constexpr (reg == KON) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                value |= voices[i].KON << i;
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                value |= voices[i + 16].KON << i;
            }
        }
    } else if constexpr (reg == NON) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                value |= voices[i].NON << i;
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                value |= voices[i + 16].NON << i;
            }
        }
    } else if constexpr (reg == FM) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                value |= voices[i].FM << i;
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                value |= voices[i + 16].FM << i;
            }
        }
    } else if constexpr (reg == Reverb) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                value |= voices[i].Reverb << i;
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                value |= voices[i + 16].Reverb << i;
            }
        }
    } else if constexpr (reg == Status) {
        if constexpr (first) {
            for (auto i = 0; i < 16; i++) {
                value |= voices[i].Status << i;
            }
        } else {
            for (auto i = 0; i < 8; i++) {
                value |= voices[i + 16].Status << i;
            }
        }
    }

    return value;
}

}  // namespace Spu

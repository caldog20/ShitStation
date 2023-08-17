#pragma once
#include <vector>

#include "support/helpers.hpp"
#include "support/log.hpp"

namespace Spu {

struct Voice {
    u16 VolumeLeft;
    u16 VolumeRight;
    u16 ADPCMSampleRate;
    u16 ADPCMStartAddress;
    u16 AD;
    u16 SR;
    u16 ASDRVolume;
    u16 ADPCMRepeatAddress;
    u32 CVolumeLR;

    bool KON;
    bool KOFF;
    bool FM;
    bool NON;
    bool Reverb;
    bool Status;
};

struct Control {
    u16 MainVolumeLeft;
    u16 MainVolumeRight;
    u16 ReverbOutputVolumeLeft;
    u16 ReverbOutputVolumeRight;
    u16 RWASA;
    u16 IRQAddress;
    u16 DataTransferAddress;
    u16 DataTransferFifo;
    u16 SPUCNT;
    u16 DataTransferControl;
    u16 SPUSTAT;
    u16 CDVolumeLeft;
    u16 CDVolumeRight;
    u16 CurrentVolumeLeft;
    u16 CurrentVolumeRight;
    u16 ExternVolumeLeft;
    u16 ExternVolumeRight;
};





class Spu {
  public:
    Spu();
    ~Spu();

    void reset();

    u8 read8(u32 address);
    u16 read16(u32 address);
    u32 read32(u32 address);
    void write8(u32 address, u8 value);
    void write16(u32 address, u16 value);
    void write32(u32 address, u32 value);


    enum class VoiceRegister {KON, KOFF, NON, Reverb, FM, Status};


    template <VoiceRegister reg, bool first>
    void setVoiceRegister(u16 value);

    template <VoiceRegister reg, bool first>
    u16 getVoiceRegister();

    u16 readRAM();
    void pushFifo(u16 value);

  private:
    Voice voices[24];
    Control control;
    std::vector<u8> spuram;

    u32 currentAddress = 0;
};

}  // namespace Spu

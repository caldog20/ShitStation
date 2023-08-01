#pragma once
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <queue>
#include <vector>

#include "BitField.hpp"
#include "cdrom_util.hpp"
#include "support/helpers.hpp"

namespace Scheduler {
class Scheduler;
}

namespace CDROM {

union StatusCode {
    u8 r;
    BitField<0, 1, u8> Error;
    BitField<1, 1, u8> Motor;
    BitField<2, 1, u8> SeekError;
    BitField<3, 1, u8> IDError;
    BitField<4, 1, u8> ShellOpen;
    BitField<5, 1, u8> Read;
    BitField<6, 1, u8> Seek;
    BitField<7, 1, u8> Play;
};

union Index {
    u8 r;
    BitField<0, 2, u8> Index;
    BitField<2, 1, u8> XAFifoEmpty;            // 1 when playing XA-ADPCM sound
    BitField<3, 1, u8> ParamFifoEmpty;         // set before writing first byte
    BitField<4, 1, u8> ParamFifoWriteReady;    // set after writing 16 bytes
    BitField<5, 1, u8> ResponseFifoReadReady;  // set after reading LAST byte
    BitField<6, 1, u8> DataFifoReadReady;      // set after reading LAST byte
    BitField<7, 1, u8> Busy;                   // 1 = busy
};

union Mode {
    u8 r;
    BitField<0, 1, u8> CDDA;
    BitField<1, 1, u8> AutoPause;
    BitField<2, 1, u8> Report;
    BitField<3, 1, u8> XAFilter;
    BitField<4, 1, u8> IgnoreBit;
    BitField<5, 1, u8> SectorSize;
    BitField<6, 1, u8> XAEnabled;
    BitField<7, 1, u8> Speed;
};

union Request {
    u8 r;
    BitField<0, 5, u8> Unused;
    BitField<5, 1, u8> SMEN;
    BitField<6, 1, u8> BFWR;
    BitField<7, 1, u8> WantData;
};

// union IRQFlags

enum CDROMStatusCodes : u8 { Error = 0, Motor, SeekError, IDError, ShellOpen, Read, Seek, Play };

enum InterruptCause : u8 { INT0 = 0, INT1, INT2, INT3, INT4, INT5, INT6, INT7 };

class CDROM {
  private:
    enum class Commands : u8 {
        Sync = 0x00,
        GetStat = 0x01,
        SetLoc = 0x02,
        Play = 0x03,
        Forward = 0x04,
        Backward = 0x05,
        ReadN = 0x06,
        MotonOn = 0x07,
        Stop = 0x08,
        Pause = 0x09,
        Init = 0x0A,
        Mute = 0x0B,
        Demute = 0x0C,
        SetFilter = 0x0D,
        SetMode = 0x0E,
        GetParam = 0x0F,
        GetLocL = 0x10,
        GetLocP = 0x11,
        SetSession = 0x12,
        GetTN = 0x13,
        GetTD = 0x14,
        SeekL = 0x15,
        SeekP = 0x16,
        SetClock = 0x17,
        GetClock = 0x18,
        Test = 0x19,
        GetID = 0x1A,
        ReadS = 0x1B,
        Reset = 0x1C,
        GetQ = 0x1D,
        ReadTOC = 0x1E,
        VideoCD = 0x1F,
        Secret1 = 0x50,
        Secret2 = 0x51,
        Secret3 = 0x52,
        Secret4 = 0x53,
        Secret5 = 0x54,
        Secret6 = 0x55,
        Secret7 = 0x56,
        SecretLock = 0x57,
        None = 0xfe,
    };

  public:
    explicit CDROM(Scheduler::Scheduler& scheduler);

    void init();
    void reset();
    u32 dmaRead();

    u8 read(u32 offset);
    u8 read0();
    u8 read1();
    u8 read2();
    u8 read3();

    void write(u32 offset, u8 value);
    void write0(u8 value);
    void write1(u8 value);
    void write2(u8 value);
    void write3(u8 value);

    void newCommand(u8 value);
    void tryStartCommand();
    void tryFinishCommand();

    void scheduleRead();
    void scheduleInterrupt(u32 cycles);
    void scheduleCommandFinish(u32 cycles);
    void scheduleStartCommand(u32 cycles);

    void setTray(bool open) {
        if (m_trayOpen != open) {
            m_trayChanged = true;
            m_trayOpen = open;
        }
    }

    void loadDisc(const std::filesystem::path& path);
    void unloadDisc() {
        m_discLoaded = false;
        m_discImage.close();
    }

    void readSector();
    void paramFifoStatus();
    void dataFifoStatus();
    void responseFifoStatus();

    void startCommand(u8 command = 0);

    static constexpr u32 durationToCycles(std::chrono::nanoseconds duration) { return duration.count() * 33868800 / 1'000'000'000; }

    Index m_status;
    Mode m_mode;
    StatusCode m_statusCode;
    Request m_request;

    Commands m_command = Commands::None;
    Commands m_pendingCommand = Commands::None;

    u8 m_irqEnable;
    u8 m_irqFlags;

    u8 av_left_cd_left_spu;
    u8 av_left_cd_right_spu;
    u8 av_right_cd_right_spu;
    u8 av_right_cd_left_spu;

    u64 m_cycles = 0;
    u32 m_cycleDelta = 0;
    u32 m_lsn = 0;
    u32 m_setlocLSN = 0;

    u8 readDataByte();

    bool m_discLoaded = false;
    bool m_trayOpen = false;
    bool m_trayChanged = false;

    bool m_updateLoc = false;

    std::ifstream m_discImage;

    LocationTarget m_location;

    enum class State { Idle, Read, Play, Seek, Busy } m_state = State::Idle;
    enum class Response { First, Second } m_currentResponse = Response::First;

    std::queue<InterruptCause> m_ints;

  private:
    Scheduler::Scheduler& scheduler;

    const std::array<u8, 4> c_version = {0x94, 0x09, 0x19, 0xc0};
    const std::array<u8, 2> c_trayOpen = {0x11, 0x80};
    const std::array<u8, 4> c_noDisk = {0x08, 0x40, 0x0, 0x0};
    const std::array<u8, 8> c_modChip = {0x02, 0x00, 0x00, 0x00, 0x53, 0x43, 0x45, 0x41};
    //    const std::array<u8, 7> c_licMode2 = {0x00, 0x20, 0x00, 0x53, 0x43, 0x45, 0x41};
    const std::array<u8, 7> c_licMode2 = {0x00, 0x20, 0x00, 'S', 'C', 'E', 'A'};

    void cdInit();
    void cdGetStat();
    void cdGetID();
    void cdReadN();
    void cdReadS();
    void cdSetMode();
    void cdSetLoc();
    void cdReadTOC();
    void cdSeekL();
    void cdPause();
    void cdTest();
    void cdDemute();

    std::queue<u8> m_responseFifo;
    std::queue<u8> m_secondResponse;
    std::queue<u8> m_paramFifo;
    std::vector<u8> m_dataFifo;
    std::vector<u8> m_sector;
    //    std::array<u8, 2352> m_sectorBuffer;
    //    std::array<u8, 2352> m_sectorBuffer;
    u32 m_dataFifoIndex = 0;
    bool m_delayFirstRead = false;
    void clearParamFifo() {
        while (!m_paramFifo.empty()) m_paramFifo.pop();
    }

    //    void clearResponseFifo() {
    //            while (!m_responseFifo.empty()) m_responseFifo.pop();
    //    }
};

}  // namespace CDROM

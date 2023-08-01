#include <SDL.h>

#include <unordered_map>

#include "BitField.hpp"
#include "scheduler/scheduler.hpp"
#include "support/fifo.hpp"
#include "support/helpers.hpp"

// TODO: Refactor SIO/Pads
namespace SIO {
class SIO;
}

class Pad {
  public:
    Pad(SIO::SIO& sio);

    void init();
    void reset();
    void keyCallback(SDL_KeyboardEvent& event);

    enum class ControllerType : u16 {
        None = 0x0,
        Analog = 0x1,
        Digital = 0x5A41,
    };

    enum class Status {
        Idle,
        Connected,
        Transferring,
    };

    Status m_status = Status::Idle;
    ControllerType m_type = ControllerType::None;

  private:
    SIO::SIO& sio;
    void setIdle();
    void transfer(u8 data);
    void ack();
    void initKeyCodes();

    bool m_ack = false;

    std::unordered_map<SDL_Keycode, u32> m_buttonLUT;

    u16 m_buttons = 0xFFFF;  // All buttons released
                             //    DigitalPadInputs m_buttons = {.buttons=0xFFFF}; // All buttons released

    friend class SIO::SIO;

    enum DigitalPadInputs : u16 {
        Up = (1 << 4),
        Down = (1 << 6),
        Left = (1 << 7),
        Right = (1 << 5),
        Select = (1 << 0),
        Start = (1 << 3),
        Cross = (1 << 14),
        Circle = (1 << 13),
        Triangle = (1 << 12),
        Square = (1 << 15),
        L1 = (1 << 10),
        R1 = (1 << 11),
        L2 = (1 << 8),
        R2 = (1 << 9),
        L3 = (1 << 1),
        R3 = (1 << 2)
    };

    enum Commands : u8 {
        Initialize = 0x01,
        Read = 0x42,
        Tap = 0x00,
        Mot = 0x00,
    };
};

namespace SIO {

template <typename T>
union Stat {
    T r;
    BitField<0, 1, T> TXReady1;       // 1=Ready/Started
    BitField<1, 1, T> FifoNotEmpty;   // 0=Empty, 1=Not Empty
    BitField<2, 1, T> TXReady2;       // 1=Ready/Finished
    BitField<3, 1, T> RXParityError;  // 0=No, 1=Error; Wrong Parity, when enabled - sticky
    BitField<4, 1, T> Overrun;        // Placeholder for SIO1
    BitField<5, 1, T> RXBadStopBit;   // Placeholder for SIO1
    BitField<6, 1, T> RXInputLevel;   // Placeholder for SIO1
    BitField<7, 1, T> AckInputLevel;
    BitField<8, 1, T> CTS;  // Placeholder for SIO1
    BitField<9, 1, T> IRQ;  // 0=None, 1=IRQ7) (See JOY_CTRL.Bit4,10-12 - sticky
    BitField<10, 1, T> zero;
    BitField<11, 21, T> BaudTimer;  // Decrements at 33MHz
};

template <typename T>
union Mode {
    T r;
    BitField<0, 2, T> BaudReloadFactor;  // 1=MUL1, 2=MUL16, 3=MUL64) (or 0=MUL1, too
    BitField<2, 2, T> CharLen;           // 0=5bits, 1=6bits, 2=7bits, 3=8bits
    BitField<4, 1, T> ParityEnable;      // 0=Disable, 1=Enable
    BitField<5, 1, T> ParityType;        // 0=Even, 1=Odd???
    BitField<6, 2, T> zero;
    BitField<8, 1, T> ClockPolarity;  // 0=Normal:High=Idle, 1=Inverse:Low=Idle
    BitField<9, 7, T> zero2;
};

template <typename T>
union Control {
    T r;
    BitField<0, 1, T> TXEN;                 // 0=Disable, 1=Enable
    BitField<1, 1, T> JoyNOutput;           // 0=High, 1=Low/Select (/JOYn as defined in Bit13)
    BitField<2, 1, T> RXEN;                 // 0=Normal, when /JOYn=Low, 1=Force Enable Once
    BitField<3, 1, T> TXOutputLevel;        // Placeholder for SIO1
    BitField<4, 1, T> Ack;                  // 0=No change, 1=Reset JOY_STAT.Bits 3,9
    BitField<5, 1, T> RTS;                  // Placeholder for SIO1
    BitField<6, 1, T> Reset;                // 0=No change, 1=Reset most JOY_registers to zero
    BitField<7, 1, T> Factor;               // Placeholder for SIO1
    BitField<8, 2, T> RXInterruptMode;      // 0..3 = IRQ when RX FIFO contains 1,2,4,8 bytes
    BitField<10, 1, T> TXInterruptEnable;   // 0=Disable, 1=Enable -  when JOY_STAT.0-or-2 ;Ready
    BitField<11, 1, T> RXInterruptEnable;   // 0=Disable, 1=Enable - when N bytes in RX FIFO
    BitField<12, 1, T> AckInterruptEnable;  // 0=Disable, 1=Enable - when JOY_STAT.7 - ACK=LOW
    BitField<13, 1, T> Slot;                // 0=/JOY1, 1=/JOY2 - set to LOW when Bit1=1
    BitField<14, 2, T> zero;
};

struct SIORegisters {
    u8 txData;       // Holds on byte to be transferred
    u8 rxData;       // Holds one byte popped from FIFO
    Stat<u32> stat;  // Read Only
    Mode<u16> mode;
    Control<u16> control;
    u16 baud;
};

class SIO {
  public:
    explicit SIO(Scheduler::Scheduler& Scheduler);

    SIORegisters m_regs;

    void init();
    void reset();
    void checkTransfer();
    void startTransfer();
    void ack(bool ack);
    void txData(u8 data);
    void rxData();
    void setFifoStatus();
    void writeControl(u16 value);
    void scheduleIRQ();

    template <typename T = u32>
    T read(u32 offset);

    template <typename T = u32>
    void write(u32 offset, T value);

    enum class DeviceType : u16 { Pad = 0x1, MemoryCard = 0x81, None };

    DeviceType m_deviceType = DeviceType::None;

    bool m_ack = false;
    Fifo<u8, 8> m_fifo;
    Pad pad;

  private:
    friend class Pad;
    Scheduler::Scheduler& m_scheduler;
};

}  // namespace SIO

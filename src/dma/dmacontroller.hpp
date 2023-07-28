#pragma once
#include "support/helpers.hpp"

namespace Bus {
class Bus;
}
namespace Scheduler {
class Scheduler;
}

namespace DMA {

enum Port : u32 { MDECIn = 0, MDECOut = 1, GPU = 2, CDROM = 3, SPU = 4, PIO = 5, OTC = 6 };
enum SyncMode : u32 { Manual = 0, Request = 1, LinkedList = 2 };
enum Direction : u32 { ToRam = 0, FromRam = 1 };
enum Step : u32 { Increment = 0, Decrement = 1 };

struct Channel {
    Direction direction;
    Step step;
    SyncMode sync;
    bool trigger;
    bool chop;
    u8 chopDMASize;
    u8 chopCPUSize;
    bool start;

    u32 base;
    u16 blockSize;
    u16 blockCount;
};

struct DICR {
    bool forceIRQ;
    u8 im;
    bool masterIRQEnable;
    u8 ip;
    bool masterIRQFlag;
};

class DMA {
  public:
    explicit DMA(Bus::Bus& bus, Scheduler::Scheduler& scheduler);

    void reset();

    u32 read(u32 address);
    void write(u32 address, u32 value);

    void write8(u32 offset, u8 value);

  private:
    void checkIRQ();

    void startDMA(Channel& channel, Port port);
    void checkChannelActive(Port port);

    void dmaLinkedList(Channel& channel, Port port);
    void dmaBlockCopy(Channel& channel, Port port);

    void transferFinished(Channel& channel, Port port);
    u32 getTransferSize(Channel& channel);

    Bus::Bus& bus;
    Scheduler::Scheduler& scheduler;

    u32 dpcr;
    DICR dicr;
    Channel channels[7];
};

}  // namespace DMA

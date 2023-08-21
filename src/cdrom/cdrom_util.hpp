#pragma once

#include "BitField.hpp"
#include "support/helpers.hpp"

namespace CDROM {

struct MSF {
    u8 m;
    u8 s;
    u8 f;

    MSF(u8 m, u8 s, u8 f) : m(m), s(s), f(f) {}
    MSF() : MSF(0, 0, 0) {}

    constexpr int toLBA() { return (m * 60 + s) * 75 + f; }

    constexpr int toLSN() { return ((m * 60 + s) * 75 + f) - 150; }
};

union Sector {
    u8 raw[2352];
    struct {
        u8 sync[12];

        union {
            u8 header_raw[4];
            struct {
                u8 m, s, f;
                u8 mode;
            } header;
        };
        u8 subheader[4];
        u8 subheader_copy[4];
        u8 data[2324];
    };
};

static int bcdtoint(u8 value) { return value - 6 * (value >> 4); }

struct LocationTarget {
    u8 min;
    u8 sec;
    u8 sect;

    u32 toLBA() {
        u32 LBA = (min * 60 + sec) * 75 + sect;
        fmt::print("LBA: {}\n", LBA);
        return LBA;
    }

    u32 toLSN() { return ((min * 60 + sec) * 75 + sect) - 150; }

    LocationTarget() : min(0), sec(0), sect(0) {}
    LocationTarget(u8 m, u8 s, u8 f) : min(fromBCD(m)), sec(fromBCD(s)), sect(fromBCD(f)) {}

    //    u8 fromBCD(u8 value) { return (value & 0xF) + ((value >> 4) * 10); }
    u8 fromBCD(u8 value) { return (value >> 4) * 10 + (value & 15); }

    void setM(u8 m) { min = fromBCD(m); }

    void setS(u8 s) { sec = fromBCD(s); }

    void setF(u8 f) { sect = fromBCD(f); }

    void set(u8 m, u8 s, u8 f) {
        setM(m);
        setS(s);
        setF(f);
    }
};

// Maybe have this asyncronously load the disc?
class CDImage {
  public:
    CDImage() {}
    CDImage(const std::filesystem::path& file) {}
    ~CDImage() {}

    void reset() {
        msf.set(0, 0, 0);
        lsn = 0;
        isSeeked = false;
        sector.clear();
    }

    void read() {
        if (!isSeeked) seek();
        sector.clear();
        std::copy(disc.begin() + lsn, disc.begin() + lsn + sectorSize, std::back_inserter(sector));
        lsn += sectorSize;
    }

    void setLoc(u8 m, u8 s, u8 f) {
        msf.set(m, s, f);
        isSeeked = false;
    }

    void seek() {
        lsn = msf.toLSN() * sectorSize;
        isSeeked = true;
    }

    std::vector<u8> getSector() { return sector; }

    [[nodiscard]] bool isDiscLoaded() const { return discLoaded; }

    void loadDisc(const std::filesystem::path& file) {
        clearDisc();
        disc.resize(std::filesystem::file_size(file));
        std::ifstream stream(file, std::ios::binary);
        stream.unsetf(std::ios::skipws);
        stream.read(reinterpret_cast<char*>(disc.data()), disc.size());
        stream.close();
        discLoaded = true;
    }

    void clearDisc() {
        disc.clear();
        discLoaded = false;
    }

  private:
    std::vector<u8> disc;
    std::vector<u8> sector;
    LocationTarget msf;
    u32 lsn = 0;
    bool isSeeked = false;
    static constexpr u32 sectorSize = 2352;
    bool discLoaded = false;
};

}  // namespace CDROM

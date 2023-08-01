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

// class ImageReader {
//     using sectorBuff = std::vector<Sector>;
//
//   private:
//     std::future_status m_status;
//     std::future<sectorBuff>;
// };

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
};

}  // namespace CDROM

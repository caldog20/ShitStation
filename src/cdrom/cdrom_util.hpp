#pragma once
#include <future>

#include "BitField.hpp"
#include "support/helpers.hpp"
#include "support/log.hpp"

namespace CDROM {

static int bcdtoint(u8 value) { return value - 6 * (value >> 4); }

struct MSF {
    u8 min;
    u8 sec;
    u8 sect;

    constexpr u32 toLBA() {
        u32 LBA = (min * 60 + sec) * 75 + sect;
        return LBA;
    }

    constexpr u32 toLSN() { return ((min * 60 + sec) * 75 + sect) - 150; }

    MSF() : min(0), sec(0), sect(0) {}
    MSF(u8 m, u8 s, u8 f) : min(fromBCD(m)), sec(fromBCD(s)), sect(fromBCD(f)) {}

    constexpr u8 fromBCD(u8 value) { return (value >> 4) * 10 + (value & 15); }

    void setM(u8 m) { min = fromBCD(m); }

    void setS(u8 s) { sec = fromBCD(s); }

    void setF(u8 f) { sect = fromBCD(f); }

    void set(u8 m, u8 s, u8 f) {
        setM(m);
        setS(s);
        setF(f);
    }
};

class CDImage {
  public:
    CDImage() {}
    CDImage(const std::filesystem::path& file) { loadDisc(file); }
    ~CDImage() {}

    void reset() {
        msf.set(0, 0, 0);
        lsn = 0;
        seeked = false;
        sector.clear();
    }

    void read() {
        if (!seeked) seek();
        sector.clear();
        std::copy(disc.begin() + lsn, disc.begin() + lsn + sectorSize, std::back_inserter(sector));
        lsn += sectorSize;
    }

    void setLoc(u8 m, u8 s, u8 f) {
        msf.set(m, s, f);
        seeked = false;
    }

    void seek() {
        lsn = msf.toLSN() * sectorSize;
        seeked = true;
    }

    std::vector<u8> getSector() { return sector; }

    [[nodiscard]] bool isDiscLoaded() {
        // Check and only get the future once
        if (discFuture.valid()) {
            getFuture();
        }
        return discLoaded;
    }

    void getFuture() {
        try {
            disc = discFuture.get();
            discLoaded = true;
        } catch (std::exception& e) {
            Log::warn("Exception loading disc: {}\n", e.what());
            clearDisc();
        }
    }

    void asyncLoadDisc(const std::filesystem::path& file) {
        discFuture = std::async(std::launch::async, [this, file]() {
            std::vector<u8> disc;
            disc.resize(std::filesystem::file_size(file));
            std::ifstream stream(file, std::ios::binary);
            stream.unsetf(std::ios::skipws);
            stream.read(reinterpret_cast<char*>(disc.data()), disc.size());
            stream.close();
            return disc;
        });
    }

    void loadDisc(const std::filesystem::path& file) {
        clearDisc();
        asyncLoadDisc(file);
        discLoaded = true;
    }

    void clearDisc() {
        disc.clear();
        discLoaded = false;
        seeked = false;
    }

  private:
    std::vector<u8> disc;
    std::vector<u8> sector;
    MSF msf;
    u32 lsn = 0;
    bool seeked = false;
    static constexpr u32 sectorSize = 2352;
    bool discLoaded = false;
    std::future<std::vector<u8>> discFuture;
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



}  // namespace CDROM

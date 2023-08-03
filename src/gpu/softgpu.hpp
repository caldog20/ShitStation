#pragma once
#include "gpu.hpp"

namespace GPU {

class SoftGPU : public GPU {
  public:
    void reset() override;
    void init();

    void drawCommand() override;

    void transferToVram() override;
    void transferToCpu() override;
    void TransferVramToVram() override;
};

}  // namespace GPU

#pragma once
#include "gpu.hpp"

namespace GPU {

class GPU_GL final : public GPU {
  public:
    GPU_GL();
    ~GPU_GL();

    void reset() override;
    void init();

    void drawCommand() override;
    void transferToVram() override;
    void transferToCpu() override;
    void TransferVramToVram() override;
};

}  // namespace GPU

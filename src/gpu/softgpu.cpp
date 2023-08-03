#include "softgpu.hpp"

namespace GPU {

void SoftGPU::reset() { GPU::reset(); }
void SoftGPU::init() {}
void SoftGPU::drawCommand() {}
void SoftGPU::transferToVram() {}
void SoftGPU::transferToCpu() {}
void SoftGPU::TransferVramToVram() {}

}  // namespace GPU

#include "gpugl.hpp"

namespace GPU {
GPU_GL::GPU_GL() {}
GPU_GL::~GPU_GL() {}
void GPU_GL::reset() { GPU::reset(); }
void GPU_GL::init() {}
void GPU_GL::drawCommand() {}
void GPU_GL::transferToVram() {}
void GPU_GL::transferToCpu() {}
void GPU_GL::TransferVramToVram() {}
}  // namespace GPU

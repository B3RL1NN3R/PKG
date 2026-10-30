#pragma once
#include "model/ImageBuffer.h"
#include "ParallelProcessor.h"
#include <vector>

namespace Filters {
ImageBuffer boxFilter(const ImageBuffer& src, int radius, EdgeMode mode,
                      const CancellationToken& cancellation = {});
ImageBuffer gaussianFilter(const ImageBuffer& src, int radius, EdgeMode mode,
                           const CancellationToken& cancellation = {});

void convolve1D_H(const ImageBuffer& src, ImageBuffer& dst, const std::vector<float>& kernel,
                  EdgeMode mode, const CancellationToken& cancellation = {});
void convolve1D_V(const ImageBuffer& src, ImageBuffer& dst, const std::vector<float>& kernel,
                  EdgeMode mode, const CancellationToken& cancellation = {});
void convolve1D_H_parallel(const ImageBuffer& src, ImageBuffer& dst, const std::vector<float>& kernel,
                           EdgeMode mode, const CancellationToken& cancellation = {});
void convolve1D_V_parallel(const ImageBuffer& src, ImageBuffer& dst, const std::vector<float>& kernel,
                           EdgeMode mode, const CancellationToken& cancellation = {});
}

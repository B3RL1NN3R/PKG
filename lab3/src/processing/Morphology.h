#pragma once
#include "model/ImageBuffer.h"

enum class MorphOp { Erosion, Dilation, Gradient, TopHat, BlackHat };

namespace Morphology {
ImageBuffer erode(const ImageBuffer& src, int radius, EdgeMode mode);
ImageBuffer dilate(const ImageBuffer& src, int radius, EdgeMode mode);
ImageBuffer apply(const ImageBuffer& src, MorphOp op, int radius, EdgeMode mode);
}
#pragma once
#include "model/ImageBuffer.h"
#include "ParallelProcessor.h"

enum class MorphOp { Erosion, Dilation, Gradient, TopHat, BlackHat };
enum class StructuringElement { Rectangle, Ellipse, Cross };

namespace Morphology {
ImageBuffer erode(const ImageBuffer& src, int radius, EdgeMode mode,
                  StructuringElement shape = StructuringElement::Rectangle,
                  const CancellationToken& cancellation = {});
ImageBuffer dilate(const ImageBuffer& src, int radius, EdgeMode mode,
                   StructuringElement shape = StructuringElement::Rectangle,
                   const CancellationToken& cancellation = {});
ImageBuffer apply(const ImageBuffer& src, MorphOp op, int radius, EdgeMode mode,
                  StructuringElement shape = StructuringElement::Rectangle,
                  const CancellationToken& cancellation = {});
}

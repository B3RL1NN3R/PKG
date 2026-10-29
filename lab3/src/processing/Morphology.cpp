#include "Morphology.h"
#include "ParallelProcessor.h"
#include <algorithm>

namespace Morphology {

ImageBuffer erode(const ImageBuffer& src, int radius, EdgeMode mode) {
    ImageBuffer out(src.width(), src.height(), src.channels());
    int channels = src.channels();
    int width = src.width();

    auto processor = [&](const ProcessingTask& task) {
        for (int y = task.startY; y < task.endY; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    uint8_t minV = 255;
                    for (int dy = -task.radius; dy <= task.radius; ++dy) {
                        for (int dx = -task.radius; dx <= task.radius; ++dx) {
                            minV = std::min(minV, task.src->get(x + dx, y + dy, c, task.mode));
                        }
                    }
                    task.dst->set(x, y, c, minV);
                }
            }
        }
    };

    ParallelProcessor::processInParallel(src, out, radius, mode, processor);
    return out;
}

ImageBuffer dilate(const ImageBuffer& src, int radius, EdgeMode mode) {
    ImageBuffer out(src.width(), src.height(), src.channels());
    int channels = src.channels();
    int width = src.width();

    auto processor = [&](const ProcessingTask& task) {
        for (int y = task.startY; y < task.endY; ++y) {
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    uint8_t maxV = 0;
                    for (int dy = -task.radius; dy <= task.radius; ++dy) {
                        for (int dx = -task.radius; dx <= task.radius; ++dx) {
                            maxV = std::max(maxV, task.src->get(x + dx, y + dy, c, task.mode));
                        }
                    }
                    task.dst->set(x, y, c, maxV);
                }
            }
        }
    };

    ParallelProcessor::processInParallel(src, out, radius, mode, processor);
    return out;
}

ImageBuffer apply(const ImageBuffer& src, MorphOp op, int radius, EdgeMode mode) {
    int channels = src.channels();

    switch (op) {
    case MorphOp::Erosion:
        return erode(src, radius, mode);

    case MorphOp::Dilation:
        return dilate(src, radius, mode);

    case MorphOp::Gradient: {
        ImageBuffer d = dilate(src, radius, mode);
        ImageBuffer e = erode(src, radius, mode);
        ImageBuffer out(src.width(), src.height(), channels);

        const uint8_t* dData = d.data();
        const uint8_t* eData = e.data();
        uint8_t* outData = out.data();
        int totalPixels = src.width() * src.height() * channels;

        for (int i = 0; i < totalPixels; ++i) {
            int v = dData[i] - eData[i];
            outData[i] = static_cast<uint8_t>(std::clamp(v, 0, 255));
        }
        return out;
    }

    case MorphOp::TopHat: {
        ImageBuffer opened = erode(dilate(src, radius, mode), radius, mode);
        ImageBuffer out(src.width(), src.height(), channels);

        const uint8_t* srcData = src.data();
        const uint8_t* openedData = opened.data();
        uint8_t* outData = out.data();
        int totalPixels = src.width() * src.height() * channels;

        for (int i = 0; i < totalPixels; ++i) {
            int v = srcData[i] - openedData[i];
            outData[i] = static_cast<uint8_t>(std::clamp(v, 0, 255));
        }
        return out;
    }

    case MorphOp::BlackHat: {
        ImageBuffer closed = dilate(erode(src, radius, mode), radius, mode);
        ImageBuffer out(src.width(), src.height(), channels);

        const uint8_t* srcData = src.data();
        const uint8_t* closedData = closed.data();
        uint8_t* outData = out.data();
        int totalPixels = src.width() * src.height() * channels;

        for (int i = 0; i < totalPixels; ++i) {
            int v = closedData[i] - srcData[i];
            outData[i] = static_cast<uint8_t>(std::clamp(v, 0, 255));
        }
        return out;
    }
    }
    return src;
}

} // namespace Morphology
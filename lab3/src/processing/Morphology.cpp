#include "Morphology.h"
#include "ParallelProcessor.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>
#include <QFuture>
#include <QVector>
#include <QtConcurrent>

namespace Morphology {
namespace {

// Linear-time sliding min/max. Rectangle kernels are separable, so runtime is
// O(width * height), independent of the kernel area.
void rectangularPass(const ImageBuffer& src, ImageBuffer& dst, int radius,
                     EdgeMode mode, bool minimum, bool horizontal, const CancellationToken& cancellation)
{
    const int lineCount = horizontal ? src.height() : src.width();
    const int lineLength = horizontal ? src.width() : src.height();
    const int channels = src.channels();
    uint8_t* dstData = dst.data();
    const int workers = std::min(ParallelProcessor::getOptimalThreadCount(), lineCount);
    const int linesPerWorker = std::max(1, (lineCount + workers - 1) / workers);
    QVector<QFuture<void>> futures;

    for (int worker = 0; worker < workers; ++worker) {
        const int firstLine = worker * linesPerWorker;
        const int lastLine = std::min(lineCount, firstLine + linesPerWorker);
        if (firstLine >= lastLine) break;
        futures.append(QtConcurrent::run([&, firstLine, lastLine]() {
            const int window = 2 * radius + 1;
            for (int line = firstLine; line < lastLine; ++line) {
                if (isCancellationRequested(cancellation)) return;
                for (int c = 0; c < channels; ++c) {
                    std::deque<int> candidates;
                    // Stream includes radius samples on either side; get()
                    // applies the selected border policy consistently.
                    for (int i = 0; i < lineLength + 2 * radius; ++i) {
                        if ((i & 0xFF) == 0 && isCancellationRequested(cancellation)) return;
                        const int coord = i - radius;
                        const uint8_t value = horizontal
                            ? src.get(coord, line, c, mode)
                            : src.get(line, coord, c, mode);

                        while (!candidates.empty()) {
                            const int tail = candidates.back();
                            const int tailCoord = tail - radius;
                            const uint8_t tailValue = horizontal
                                ? src.get(tailCoord, line, c, mode)
                                : src.get(line, tailCoord, c, mode);
                            if (minimum ? tailValue >= value : tailValue <= value)
                                candidates.pop_back();
                            else
                                break;
                        }
                        candidates.push_back(i);
                        const int windowStart = i - window + 1;
                        while (!candidates.empty() && candidates.front() < windowStart)
                            candidates.pop_front();

                        if (i >= window - 1) {
                            const int outCoord = i - (window - 1);
                            const int bestCoord = candidates.front() - radius;
                            const uint8_t best = horizontal
                                ? src.get(bestCoord, line, c, mode)
                                : src.get(line, bestCoord, c, mode);
                            const int outX = horizontal ? outCoord : line;
                            const int outY = horizontal ? line : outCoord;
                            dstData[(outY * dst.width() + outX) * channels + c] = best;
                        }
                    }
                }
            }
        }));
    }
    for (auto& future : futures) future.waitForFinished();
}



ImageBuffer morphology(const ImageBuffer& src, int radius, EdgeMode mode,
                       StructuringElement shape, bool minimum, const CancellationToken& cancellation)
{
    if (src.width() <= 0 || src.height() <= 0 || src.channels() <= 0) return src;
    radius = std::clamp(radius, 0, 128);

    if (shape == StructuringElement::Rectangle) {
        ImageBuffer horizontal(src.width(), src.height(), src.channels());
        ImageBuffer result(src.width(), src.height(), src.channels());
        rectangularPass(src, horizontal, radius, mode, minimum, true, cancellation);
        if (isCancellationRequested(cancellation)) return ImageBuffer();
        rectangularPass(horizontal, result, radius, mode, minimum, false, cancellation);
        return isCancellationRequested(cancellation) ? ImageBuffer() : result;
    }

    ImageBuffer out(src.width(), src.height(), src.channels());
    const int width = src.width();
    const int channels = src.channels();
    auto processor = [&](const ProcessingTask& task) {
        for (int y = task.startY; y < task.endY; ++y) {
            if (isCancellationRequested(task.cancellation)) return;
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    uint8_t value = minimum ? 255 : 0;
                    if (shape == StructuringElement::Cross) {
                        // A cross is the union of one horizontal and one vertical
                        // segment; visit only those 4r+1 samples, not the square.
                        for (int d = -radius; d <= radius; ++d) {
                            const uint8_t horizontal = task.src->get(x + d, y, c, task.mode);
                            const uint8_t vertical = task.src->get(x, y + d, c, task.mode);
                            value = minimum ? std::min(value, horizontal) : std::max(value, horizontal);
                            value = minimum ? std::min(value, vertical) : std::max(value, vertical);
                        }
                    } else {
                        // For an ellipse, compute the exact horizontal span for
                        // each row and skip all points outside the disk.
                        for (int dy = -radius; dy <= radius; ++dy) {
                            const int dxLimit = static_cast<int>(std::floor(
                                std::sqrt(static_cast<double>(radius * radius - dy * dy))));
                            for (int dx = -dxLimit; dx <= dxLimit; ++dx) {
                                const uint8_t sample = task.src->get(x + dx, y + dy, c, task.mode);
                                value = minimum ? std::min(value, sample) : std::max(value, sample);
                            }
                        }
                    }
                    task.dstData[(y * width + x) * channels + c] = value;
                }
            }
        }
    };
    ParallelProcessor::processInParallel(src, out, radius, mode, processor, cancellation);
    return isCancellationRequested(cancellation) ? ImageBuffer() : out;
}

ImageBuffer subtractClamped(const ImageBuffer& a, const ImageBuffer& b, const CancellationToken& cancellation)
{
    ImageBuffer out(a.width(), a.height(), a.channels());
    const int total = a.width() * a.height() * a.channels();
    const uint8_t* ad = a.data();
    const uint8_t* bd = b.data();
    uint8_t* od = out.data();
    for (int i = 0; i < total; ++i) {
        if ((i & 0x3FFF) == 0 && isCancellationRequested(cancellation)) return ImageBuffer();
        od[i] = static_cast<uint8_t>(std::max(0, static_cast<int>(ad[i]) - static_cast<int>(bd[i])));
    }
    return out;
}
}

ImageBuffer erode(const ImageBuffer& src, int radius, EdgeMode mode, StructuringElement shape, const CancellationToken& cancellation)
{ return morphology(src, radius, mode, shape, true, cancellation); }

ImageBuffer dilate(const ImageBuffer& src, int radius, EdgeMode mode, StructuringElement shape, const CancellationToken& cancellation)
{ return morphology(src, radius, mode, shape, false, cancellation); }

ImageBuffer apply(const ImageBuffer& src, MorphOp op, int radius, EdgeMode mode, StructuringElement shape, const CancellationToken& cancellation)
{
    switch (op) {
    case MorphOp::Erosion: return erode(src, radius, mode, shape, cancellation);
    case MorphOp::Dilation: return dilate(src, radius, mode, shape, cancellation);
    case MorphOp::Gradient:
        { ImageBuffer dilated = dilate(src, radius, mode, shape, cancellation);
        if (isCancellationRequested(cancellation)) return ImageBuffer();
        ImageBuffer eroded = erode(src, radius, mode, shape, cancellation);
        if (isCancellationRequested(cancellation)) return ImageBuffer();
        return subtractClamped(dilated, eroded, cancellation); }
    case MorphOp::TopHat: {
        // Opening = erosion followed by dilation.
        // White Top-Hat = source - opening: extracts bright details smaller
        // than the structuring element.
        ImageBuffer eroded = erode(src, radius, mode, shape, cancellation);
        if (isCancellationRequested(cancellation)) return ImageBuffer();
        ImageBuffer opened = dilate(eroded, radius, mode, shape, cancellation);
        if (isCancellationRequested(cancellation)) return ImageBuffer();
        return subtractClamped(src, opened, cancellation);
    }
    case MorphOp::BlackHat: {
        // Closing = dilation followed by erosion.
        // Black-Hat = closing - source: extracts dark details.
        ImageBuffer dilated = dilate(src, radius, mode, shape, cancellation);
        if (isCancellationRequested(cancellation)) return ImageBuffer();
        ImageBuffer closed = erode(dilated, radius, mode, shape, cancellation);
        if (isCancellationRequested(cancellation)) return ImageBuffer();
        return subtractClamped(closed, src, cancellation);
    }
    }
    return src;
}
} // namespace Morphology

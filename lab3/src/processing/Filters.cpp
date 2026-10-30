#include "Filters.h"
#include "ParallelProcessor.h"
#include <cmath>
#include <algorithm>

namespace Filters {

void convolve1D_H(const ImageBuffer& src, ImageBuffer& dst, const std::vector<float>& kernel, EdgeMode mode, const CancellationToken& cancellation) {
    int k = static_cast<int>(kernel.size());
    int half = k / 2;
    int width = src.width();
    int height = src.height();
    int channels = src.channels();

    for (int y = 0; y < height; ++y) {
        if (isCancellationRequested(cancellation)) return;
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                float sum = 0.f;
                for (int i = 0; i < k; ++i) {
                    int xi = x + i - half;
                    sum += kernel[i] * src.get(xi, y, c, mode);
                }
                dst.set(x, y, c, static_cast<uint8_t>(std::clamp(sum, 0.f, 255.f)));
            }
        }
    }
}

void convolve1D_H_parallel(const ImageBuffer& src, ImageBuffer& dst, const std::vector<float>& kernel, EdgeMode mode, const CancellationToken& cancellation) {
    int k = static_cast<int>(kernel.size());
    int half = k / 2;
    int width = src.width();
    int channels = src.channels();

    auto processor = [&](const ProcessingTask& task) {
        for (int y = task.startY; y < task.endY; ++y) {
            if (isCancellationRequested(task.cancellation)) return;
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    float sum = 0.f;
                    for (int i = 0; i < k; ++i) {
                        int xi = x + i - half;
                        sum += kernel[i] * task.src->get(xi, y, c, task.mode);
                    }
                    task.dstData[(y * width + x) * channels + c] = static_cast<uint8_t>(std::clamp(sum, 0.f, 255.f));
                }
            }
        }
    };

    ParallelProcessor::processInParallel(src, dst, 0, mode, processor, cancellation);
}

void convolve1D_V(const ImageBuffer& src, ImageBuffer& dst, const std::vector<float>& kernel, EdgeMode mode, const CancellationToken& cancellation) {
    int k = static_cast<int>(kernel.size());
    int half = k / 2;
    int width = src.width();
    int height = src.height();
    int channels = src.channels();

    for (int y = 0; y < height; ++y) {
        if (isCancellationRequested(cancellation)) return;
        for (int x = 0; x < width; ++x) {
            for (int c = 0; c < channels; ++c) {
                float sum = 0.f;
                for (int i = 0; i < k; ++i) {
                    int yi = y + i - half;
                    sum += kernel[i] * src.get(x, yi, c, mode);
                }
                dst.set(x, y, c, static_cast<uint8_t>(std::clamp(sum, 0.f, 255.f)));
            }
        }
    }
}

void convolve1D_V_parallel(const ImageBuffer& src, ImageBuffer& dst, const std::vector<float>& kernel, EdgeMode mode, const CancellationToken& cancellation) {
    int k = static_cast<int>(kernel.size());
    int half = k / 2;
    int width = src.width();
    int channels = src.channels();

    auto processor = [&](const ProcessingTask& task) {
        for (int y = task.startY; y < task.endY; ++y) {
            if (isCancellationRequested(task.cancellation)) return;
            for (int x = 0; x < width; ++x) {
                for (int c = 0; c < channels; ++c) {
                    float sum = 0.f;
                    for (int i = 0; i < k; ++i) {
                        int yi = y + i - half;
                        sum += kernel[i] * task.src->get(x, yi, c, task.mode);
                    }
                    task.dstData[(y * width + x) * channels + c] = static_cast<uint8_t>(std::clamp(sum, 0.f, 255.f));
                }
            }
        }
    };

    ParallelProcessor::processInParallel(src, dst, 0, mode, processor, cancellation);
}

ImageBuffer boxFilter(const ImageBuffer& src, int radius, EdgeMode mode, const CancellationToken& cancellation) {
    radius = std::clamp(radius, 0, 128);
    int k = 2 * radius + 1;
    std::vector<float> kernel(k, 1.f / k);
    ImageBuffer tmp(src.width(), src.height(), src.channels());
    ImageBuffer out(src.width(), src.height(), src.channels());

    convolve1D_H_parallel(src, tmp, kernel, mode, cancellation);
    if (isCancellationRequested(cancellation)) return ImageBuffer();
    convolve1D_V_parallel(tmp, out, kernel, mode, cancellation);
    if (isCancellationRequested(cancellation)) return ImageBuffer();

    return out;
}

ImageBuffer gaussianFilter(const ImageBuffer& src, int radius, EdgeMode mode, const CancellationToken& cancellation) {
    radius = std::clamp(radius, 0, 128);
    int k = 2 * radius + 1;
    std::vector<float> kernel(k);
    float sigma = std::max(0.3f, static_cast<float>(radius) / 2.f);
    float sum = 0.f;

    for (int i = 0; i < k; ++i) {
        float x = i - radius;
        kernel[i] = std::exp(-(x * x) / (2.f * sigma * sigma));
        sum += kernel[i];
    }
    for (auto& v : kernel) v /= sum;

    ImageBuffer tmp(src.width(), src.height(), src.channels());
    ImageBuffer out(src.width(), src.height(), src.channels());

    convolve1D_H_parallel(src, tmp, kernel, mode, cancellation);
    if (isCancellationRequested(cancellation)) return ImageBuffer();
    convolve1D_V_parallel(tmp, out, kernel, mode, cancellation);
    if (isCancellationRequested(cancellation)) return ImageBuffer();

    return out;
}

} // namespace Filters
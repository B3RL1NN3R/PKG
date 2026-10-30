#pragma once
#include "model/ImageBuffer.h"
#include <QFuture>
#include <QVector>
#include <QtConcurrent>
#include <algorithm>
#include <atomic>
#include <memory>

using CancellationToken = std::shared_ptr<std::atomic_bool>;

inline bool isCancellationRequested(const CancellationToken& token)
{
    return token && token->load(std::memory_order_relaxed);
}

struct ProcessingTask {
    int startY;
    int endY;
    const ImageBuffer* src;
    ImageBuffer* dst;
    uint8_t* dstData;
    int radius;
    EdgeMode mode;
    CancellationToken cancellation;
};

namespace ParallelProcessor {
int getOptimalThreadCount();

template<typename Func>
void processInParallel(const ImageBuffer& src, ImageBuffer& dst,
                       int radius, EdgeMode mode, Func processor,
                       const CancellationToken& cancellation = {}) {
    if (isCancellationRequested(cancellation)) return;

    const int threadCount = std::min(getOptimalThreadCount(), std::max(1, src.height()));
    uint8_t* dstData = dst.data();
    const qint64 pixelCount = static_cast<qint64>(src.width()) * src.height();

    if (pixelCount < 262144 || threadCount <= 1) {
        ProcessingTask task{0, src.height(), &src, &dst, dstData, radius, mode, cancellation};
        processor(task);
        return;
    }

    const int rowsPerThread = (src.height() + threadCount - 1) / threadCount;
    QVector<QFuture<void>> futures;
    futures.reserve(threadCount);

    for (int i = 0; i < threadCount; ++i) {
        if (isCancellationRequested(cancellation)) break;
        const int startY = i * rowsPerThread;
        const int endY = std::min(startY + rowsPerThread, src.height());
        if (startY >= src.height()) break;

        ProcessingTask task{startY, endY, &src, &dst, dstData, radius, mode, cancellation};
        futures.append(QtConcurrent::run([task, processor]() { processor(task); }));
    }

    // Never return while a worker still holds references to src/dst or processor.
    for (auto& future : futures) future.waitForFinished();
}
} // namespace ParallelProcessor

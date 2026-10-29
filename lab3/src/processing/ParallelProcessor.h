#pragma once
#include "model/ImageBuffer.h"
#include <QFuture>
#include <QVector>
#include <QtConcurrent>

struct ProcessingTask {
    int startY;
    int endY;
    const ImageBuffer* src;
    ImageBuffer* dst;
    int radius;
    EdgeMode mode;
};

namespace ParallelProcessor {
int getOptimalThreadCount();

template<typename Func>
void processInParallel(const ImageBuffer& src, ImageBuffer& dst,
                       int radius, EdgeMode mode, Func processor) {
    int threadCount = getOptimalThreadCount();
    int rowsPerThread = (src.height() + threadCount - 1) / threadCount;

    QVector<QFuture<void>> futures;
    futures.reserve(threadCount);

    for (int i = 0; i < threadCount; ++i) {
        int startY = i * rowsPerThread;
        int endY = std::min(startY + rowsPerThread, src.height());

        if (startY >= src.height()) break;

        ProcessingTask task{startY, endY, &src, &dst, radius, mode};

        auto future = QtConcurrent::run([task, processor]() {
            processor(task);
        });
        futures.append(future);
    }

    for (auto& future : futures) {
        future.waitForFinished();
    }
}
}
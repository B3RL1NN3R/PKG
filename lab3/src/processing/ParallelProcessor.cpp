#include "ParallelProcessor.h"
#include <QThread>
#include <QThreadPool>
#include <algorithm>

namespace ParallelProcessor {

int getOptimalThreadCount() {
    const int ideal = QThread::idealThreadCount();
    const int poolLimit = QThreadPool::globalInstance()->maxThreadCount();
    return std::clamp(std::min(ideal, poolLimit), 1, 16);
}

} // namespace ParallelProcessor

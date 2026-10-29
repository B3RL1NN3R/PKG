#include "ParallelProcessor.h"
#include <QThread>

namespace ParallelProcessor {

int getOptimalThreadCount() {
    int cores = QThread::idealThreadCount();
    return std::max(1, cores);
}

} // namespace ParallelProcessor
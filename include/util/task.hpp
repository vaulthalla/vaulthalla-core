#pragma once

#include <vector>
#include <utility>

namespace vh::concurrency {

using opRange = std::pair<unsigned int, unsigned int>;

inline std::vector<opRange> getTaskOperationRanges(const unsigned int totalOperations, const unsigned int maxThreads) {
    std::vector<opRange> taskOperationRanges;
    taskOperationRanges.reserve(totalOperations);

    const unsigned int operationsPerTask = totalOperations / maxThreads;
    const unsigned int remainder = totalOperations % maxThreads;

    // end should be inclusive
    for (unsigned int i = 0; i < maxThreads; ++i) {
        unsigned int start = i * operationsPerTask;
        unsigned int end = start + operationsPerTask - 1;
        if (i < remainder) end++;
        if (start < totalOperations) taskOperationRanges.emplace_back(start, end);
    }

    if (taskOperationRanges.empty() || taskOperationRanges.back().second < totalOperations - 1)
        taskOperationRanges.emplace_back(taskOperationRanges.back().second + 1, totalOperations - 1);

    return taskOperationRanges;
}

}

#pragma once

#include "concurrency/ThreadPool.hpp"  // Assuming you’ve got one
#include <fuse_lowlevel.h>

using namespace vh::concurrency;

namespace vh::fuse {

class FUSELoopRunner {
public:
    bool run();

private:
    std::unique_ptr<ThreadPool> threadPool_;
    fuse_session* session_{nullptr};
};

}

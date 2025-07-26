#pragma once

#define FUSE_USE_VERSION 35

#include <memory>
#include <fuse_lowlevel.h>
#include <thread>
#include <atomic>

namespace vh::storage {
class StorageManager;
}

namespace vh::fuse {

class FUSEBridge;

class FUSELoopRunner {
public:
    explicit FUSELoopRunner(const std::shared_ptr<storage::StorageManager>& storageManager);

    void run();

    void stop();

    [[nodiscard]] fuse_session* session() const noexcept { return session_; }

private:
    std::shared_ptr<FUSEBridge> bridge_;
    fuse_session* session_{nullptr};
    std::atomic_bool running_{false};
    std::thread loopThread_;

    void fuseLoop();
};

}

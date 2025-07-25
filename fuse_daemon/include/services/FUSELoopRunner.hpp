#pragma once

#define FUSE_USE_VERSION 35

#include <memory>
#include <fuse_lowlevel.h>

namespace vh::storage {
class StorageManager;
}

namespace vh::fuse {

class FUSEBridge;

class FUSELoopRunner {
public:
    explicit FUSELoopRunner(const std::shared_ptr<storage::StorageManager>& storageManager);

    bool run();

private:
    std::shared_ptr<FUSEBridge> bridge_;
    fuse_session* session_{nullptr};
};

}

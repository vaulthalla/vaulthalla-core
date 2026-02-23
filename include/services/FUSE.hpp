#pragma once

#define FUSE_USE_VERSION 35

#include "concurrency/AsyncService.hpp"

#include <fuse_lowlevel.h>

namespace vh::storage {
class StorageManager;
}

namespace vh::fuse {
class FUSEBridge;
}

namespace vh::services {

class FUSE final : public concurrency::AsyncService {
    friend class ServiceManager;

public:

    explicit FUSE();

    void stop() override;

    [[nodiscard]] fuse_session* session() const noexcept { return session_; }

protected:
    void runLoop() override;

private:
    fuse_session* session_{nullptr};
};

}

#pragma once

#define FUSE_USE_VERSION 35

#include "services/AsyncService.hpp"

#include <memory>
#include <fuse_lowlevel.h>

namespace vh::storage {
class StorageManager;
}

namespace vh::fuse {
class FUSEBridge;
}

namespace vh::services {

class FUSE final : public AsyncService {
public:
    explicit FUSE(const std::shared_ptr<ServiceManager>& serviceManager);

    void stop() override;

    [[nodiscard]] fuse_session* session() const noexcept { return session_; }

protected:
    void runLoop() override;

private:
    std::shared_ptr<fuse::FUSEBridge> bridge_;
    fuse_session* session_{nullptr};
};

}

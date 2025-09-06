#pragma once

#define FUSE_USE_VERSION 35

#include "services/AsyncService.hpp"

#include <fuse_lowlevel.h>
#include <paths.h>

namespace vh::storage {
class StorageManager;
}

namespace vh::fuse {
class FUSEBridge;
}

namespace vh::services {

class FUSE final : public AsyncService {
public:
    explicit FUSE(std::filesystem::path  mount = paths::getMountPath());

    void stop() override;

    [[nodiscard]] fuse_session* session() const noexcept { return session_; }

    void setMountPoint(const std::filesystem::path& mount) { mountPoint_ = mount; }
    [[nodiscard]] const std::filesystem::path& mountPoint() const noexcept { return mountPoint_; }

protected:
    void runLoop() override;

private:
    fuse_session* session_{nullptr};
    std::filesystem::path mountPoint_;
};

}

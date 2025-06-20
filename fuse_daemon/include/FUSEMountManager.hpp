#pragma once

#include <string>

namespace vh::fuse {

class FUSEMountManager {
  public:
    explicit FUSEMountManager(std::string mountPoint);

    // Prepare mountpoint (mkdir, check not already mounted)
    bool mount();

    // Unmount the FUSE FS from the mountpoint
    bool unmount();

    // Return the current mountpoint string
    const std::string& getMountPoint() const;

  private:
    std::string mountPoint_;
};

} // namespace vh::fuse

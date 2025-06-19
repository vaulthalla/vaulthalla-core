#pragma once

#include <string>
#include <memory>

namespace vh::fuse {

    class FUSEMountManager {
    public:
        explicit FUSEMountManager(std::string mountPoint);

        bool mount();
        bool unmount();

        const std::string& getMountPoint() const;

    private:
        std::string mountPoint_;
    };

}

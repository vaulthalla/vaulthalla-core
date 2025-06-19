#include "FUSEMountManager.hpp"
#include <cstdlib>
#include <iostream>

using namespace vh::fuse;

FUSEMountManager::FUSEMountManager(std::string mountPoint)
    : mountPoint_(std::move(mountPoint)) {}

bool FUSEMountManager::mount() {
    // Optional logic before fuse_main handles it.
    return true;
}

bool FUSEMountManager::unmount() {
    std::string cmd = "fusermount3 -u " + mountPoint_;
    int ret = std::system(cmd.c_str());
    return (ret == 0);
}

const std::string& FUSEMountManager::getMountPoint() const {
    return mountPoint_;
}

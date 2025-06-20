#include "FUSEMountManager.hpp"
#include <filesystem>
#include <cstdlib>
#include <iostream>

using namespace vh::fuse;
namespace fs = std::filesystem;

FUSEMountManager::FUSEMountManager(std::string mountPoint)
    : mountPoint_(std::move(mountPoint)) {}

bool FUSEMountManager::mount() {
    // Ensure directory exists
    if (!fs::exists(mountPoint_)) {
        std::cout << "[+] Creating mountpoint: " << mountPoint_ << "\n";
        try {
            fs::create_directories(mountPoint_);
        } catch (const fs::filesystem_error& e) {
            std::cerr << "[-] Failed to create mountpoint: " << e.what() << "\n";
            return false;
        }
    }

    if (!fs::is_directory(mountPoint_)) {
        std::cerr << "[-] Mountpoint is not a directory: " << mountPoint_ << "\n";
        return false;
    }

    // Optional: check if already mounted
    std::string cmd = "mountpoint -q " + mountPoint_;
    if (std::system(cmd.c_str()) == 0) {
        std::cerr << "[-] Already mounted: " << mountPoint_ << "\n";
        return false;
    }

    std::cout << "[+] Mountpoint ready: " << mountPoint_ << "\n";
    return true;
}

bool FUSEMountManager::unmount() {
    std::string cmd = "fusermount3 -u " + mountPoint_;
    int ret = std::system(cmd.c_str());
    if (ret != 0) {
        std::cerr << "[-] Failed to unmount " << mountPoint_ << " (code " << ret << ")\n";
        return false;
    }

    std::cout << "[+] Unmounted " << mountPoint_ << "\n";
    return true;
}

const std::string& FUSEMountManager::getMountPoint() const {
    return mountPoint_;
}

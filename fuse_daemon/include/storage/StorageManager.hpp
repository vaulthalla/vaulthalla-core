#pragma once

#define FUSE_USE_VERSION 35

#include <mutex>
#include <shared_mutex>
#include <vector>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include <fuse3/fuse_lowlevel.h>

namespace vh::types {
struct FSEntry;
}

namespace fs = std::filesystem;

namespace vh::storage {

struct StorageEngine;

class StorageManager {
public:
    StorageManager();

    void initStorageEngines();

    std::vector<std::shared_ptr<types::FSEntry>> listDir(const fs::path& absPath, bool recursive = false) const;

    fuse_ino_t assignInode(const fs::path& path);

    fuse_ino_t getOrAssignInode(const fs::path& path);

    fs::path resolvePathFromInode(fuse_ino_t ino) const;

    std::vector<std::shared_ptr<StorageEngine>> getEngines() const;

private:
    mutable std::mutex mutex_;
    std::pmr::unordered_map<std::string, std::shared_ptr<StorageEngine>> engines_;

    fuse_ino_t nextInode_ = 2;
    std::unordered_map<fuse_ino_t, fs::path> inodeToPath_;
    std::unordered_map<fs::path, fuse_ino_t> pathToInode_;
    mutable std::shared_mutex inodeMutex_;

    std::shared_ptr<StorageEngine> resolveStorageEngine(const fs::path& absPath) const;
};

}

#pragma once

#include <unordered_map>
#include <memory>
#include <shared_mutex>
#include <filesystem>
#include <optional>
#include <fuse3/fuse_lowlevel.h>

namespace vh::types {
struct FSEntry;
}

namespace fs = std::filesystem;

namespace vh::storage {

struct CacheEntry {
    std::shared_ptr<types::FSEntry> entry;
    fs::path vaultPath;
    fs::path absPath;
    fuse_ino_t inode;
};

class FSCache {
public:
    FSCache();

    void cache(const std::shared_ptr<types::FSEntry>& entry);
    void evictByPath(const fs::path& vaultPath);
    void evictByInode(fuse_ino_t inode);

    std::shared_ptr<types::FSEntry> getByPath(const fs::path& vaultPath) const;
    std::shared_ptr<types::FSEntry> getByInode(fuse_ino_t inode) const;

    bool exists(const fs::path& vaultPath) const;
    fs::path resolvePathFromInode(fuse_ino_t inode) const;
    fuse_ino_t resolveInode(const fs::path& vaultPath) const;

    fuse_ino_t assignInode(const fs::path& vaultPath);

private:
    mutable std::shared_mutex mutex_;
    fuse_ino_t nextInode_ = 2;

    std::unordered_map<fs::path, CacheEntry> pathCache_;
    std::unordered_map<fuse_ino_t, CacheEntry> inodeCache_;
};

}

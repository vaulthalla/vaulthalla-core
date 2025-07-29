#include "storage/FSCache.hpp"
#include "types/FSEntry.hpp"

#include <iostream>
#include <mutex>

using namespace vh::storage;
using namespace vh::types;

FSCache::FSCache() = default;

void FSCache::cache(const std::shared_ptr<FSEntry>& entry) {
    if (!entry || !entry->inode)
        throw std::invalid_argument("Cannot cache entry: missing inode");

    std::unique_lock lock(mutex_);
    CacheEntry ce{
        .entry = entry,
        .vaultPath = entry->path,
        .absPath = entry->abs_path,
        .inode = *entry->inode
    };
    pathCache_[entry->path] = ce;
    inodeCache_[*entry->inode] = ce;

    std::cout << "[FSCache] Cached entry: " << entry->path << " -> inode " << *entry->inode << std::endl;
}

void FSCache::evictByPath(const fs::path& vaultPath) {
    std::unique_lock lock(mutex_);
    auto it = pathCache_.find(vaultPath);
    if (it != pathCache_.end()) {
        inodeCache_.erase(it->second.inode);
        pathCache_.erase(it);
    }
}

void FSCache::evictByInode(fuse_ino_t inode) {
    std::unique_lock lock(mutex_);
    auto it = inodeCache_.find(inode);
    if (it != inodeCache_.end()) {
        pathCache_.erase(it->second.vaultPath);
        inodeCache_.erase(it);
    }
}

std::shared_ptr<FSEntry> FSCache::getByPath(const fs::path& vaultPath) const {
    std::shared_lock lock(mutex_);
    auto it = pathCache_.find(vaultPath);
    return it != pathCache_.end() ? it->second.entry : nullptr;
}

std::shared_ptr<FSEntry> FSCache::getByInode(fuse_ino_t inode) const {
    std::shared_lock lock(mutex_);
    auto it = inodeCache_.find(inode);
    return it != inodeCache_.end() ? it->second.entry : nullptr;
}

bool FSCache::exists(const fs::path& vaultPath) const {
    std::shared_lock lock(mutex_);
    return pathCache_.contains(vaultPath);
}

fs::path FSCache::resolvePathFromInode(fuse_ino_t inode) const {
    std::shared_lock lock(mutex_);
    auto it = inodeCache_.find(inode);
    return it != inodeCache_.end() ? it->second.vaultPath : fs::path();
}

fuse_ino_t FSCache::resolveInode(const fs::path& vaultPath) const {
    std::shared_lock lock(mutex_);
    auto it = pathCache_.find(vaultPath);
    return it != pathCache_.end() ? it->second.inode : FUSE_ROOT_ID;
}

fuse_ino_t FSCache::assignInode(const fs::path& vaultPath) {
    std::unique_lock lock(mutex_);
    auto it = pathCache_.find(vaultPath);
    if (it != pathCache_.end()) return it->second.inode;

    fuse_ino_t ino = nextInode_++;
    CacheEntry ce{
        .entry = nullptr,
        .vaultPath = vaultPath,
        .absPath = {},
        .inode = ino
    };
    pathCache_[vaultPath] = ce;
    inodeCache_[ino] = ce;
    return ino;
}

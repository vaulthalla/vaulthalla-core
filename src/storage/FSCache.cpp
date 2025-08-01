#include "storage/FSCache.hpp"
#include "types/FSEntry.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "util/fsPath.hpp"

#include <iostream>
#include <mutex>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

FSCache::FSCache() {
    inodeToPath_[FUSE_ROOT_ID] = "/";
    pathToInode_["/"] = FUSE_ROOT_ID;
    cacheEntry(FSEntryQueries::getFSEntry("/"));
    nextInode_ = FSEntryQueries::getNextInode();
}

std::shared_ptr<FSEntry> FSCache::getEntry(const fs::path& absPath) {
    const auto path = makeAbsolute(absPath);
    std::cout << "[StorageManager] Retrieving entry for path: " << path << std::endl;
    const auto it = pathToEntry_.find(path);
    if (it != pathToEntry_.end()) return it->second;

    try {
        auto entry = FSEntryQueries::getFSEntry(path);
        if (!entry) {
            std::cerr << "[StorageManager] Entry not found for path: " << path << std::endl;
            return nullptr;
        }

        if (entry->inode) cacheEntry(entry);
        else {
            entry->inode = std::make_optional(getOrAssignInode(path));
            cacheEntry(entry);
        }
        return entry;
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error retrieving entry for path " << path << ": " << e.what() << std::endl;
        return nullptr;
    }
}

fuse_ino_t FSCache::resolveInode(const fs::path& absPath) {
    std::shared_lock lock(mutex_);
    auto it = pathToInode_.find(absPath);
    if (it != pathToInode_.end()) return it->second;

    const auto entry = FSEntryQueries::getFSEntry(absPath);
    if (!entry) {
        std::cerr << "[StorageManager] Entry not found for path: " << absPath << std::endl;
        return FUSE_ROOT_ID; // or some error code
    }

    fuse_ino_t ino = entry && entry->inode ? *entry->inode : getOrAssignInode(absPath);
    if (ino == FUSE_ROOT_ID) {
        std::cerr << "[StorageManager] Failed to resolve inode for path: " << absPath << std::endl;
        return FUSE_ROOT_ID; // or some error code
    }

    // Cache the resolved inode
    linkPath(absPath, ino);
    return ino;
}

fuse_ino_t FSCache::assignInode(const fs::path& path) {
    std::unique_lock lock(mutex_);
    const auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) return it->second;

    const fuse_ino_t ino = nextInode_++;
    inodeToPath_[ino] = path;
    pathToInode_[path] = ino;
    return ino;
}

fuse_ino_t FSCache::getOrAssignInode(const fs::path& path) {
    std::scoped_lock lock(mutex_);
    auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) return it->second;

    fuse_ino_t new_ino = nextInode_++;
    pathToInode_[path] = new_ino;
    inodeToPath_[new_ino] = path;
    return new_ino;
}

void FSCache::linkPath(const fs::path& absPath, const fuse_ino_t ino) {
    std::unique_lock lock(mutex_);
    if (pathToInode_.contains(absPath)) {
        std::cerr << "[StorageManager] Path already linked: " << absPath << std::endl;
        return; // Path already exists
    }
    pathToInode_[absPath] = ino;
    inodeToPath_[ino] = absPath;
}

fs::path FSCache::resolvePath(fuse_ino_t ino) {
    std::shared_lock lock(mutex_);
    auto it = inodeToPath_.find(ino);
    if (it == inodeToPath_.end()) throw std::runtime_error("No path for inode");
    return it->second;
}

void FSCache::decrementInodeRef(const fuse_ino_t ino, const uint64_t nlookup) {
    std::unique_lock lock(mutex_);
    auto it = inodeToPath_.find(ino);
    if (it == inodeToPath_.end()) return; // No such inode

    if (nlookup > 1) return;  // This is a no-op, but could be extended

    // If nlookup is 1, we can remove the inode
    pathToInode_.erase(it->second);
    inodeToPath_.erase(it);
    inodeToEntry_.erase(ino);
    pathToEntry_.erase(it->second);
}

void FSCache::cacheEntry(const std::shared_ptr<FSEntry>& entry) {
    if (!entry || !entry->inode) throw std::invalid_argument("Entry or inode is null");

    std::unique_lock lock(mutex_);
    inodeToPath_[*entry->inode] = entry->fuse_path;
    inodeToEntry_[*entry->inode] = entry;
    pathToInode_[entry->fuse_path] = *entry->inode;
    pathToEntry_[entry->fuse_path] = entry;
}

bool FSCache::entryExists(const fs::path& absPath) const {
    std::shared_lock lock(mutex_);
    return pathToEntry_.contains(absPath) || FSEntryQueries::exists(absPath);
}

std::shared_ptr<FSEntry> FSCache::getEntryFromInode(fuse_ino_t ino) const {
    std::shared_lock lock(mutex_);
    auto it = inodeToEntry_.find(ino);
    if (it != inodeToEntry_.end()) return it->second;
    return nullptr;
}

void FSCache::evictPath(const fs::path& path) {
    std::unique_lock lock(mutex_);
    auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) {
        fuse_ino_t ino = it->second;
        inodeToPath_.erase(ino);
        inodeToEntry_.erase(ino);
        pathToInode_.erase(path);
        pathToEntry_.erase(path);
    }
}

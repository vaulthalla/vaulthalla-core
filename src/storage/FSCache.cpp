#include "storage/FSCache.hpp"
#include "types/FSEntry.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "util/fsPath.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"
#include "crypto/IdGenerator.hpp"

#include <mutex>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::services;
using namespace vh::logging;

FSCache::FSCache() {
    inodeToPath_[FUSE_ROOT_ID] = "/";
    pathToInode_["/"] = FUSE_ROOT_ID;
    nextInode_ = FSEntryQueries::getNextInode();
    initRoot();
    restoreCache();
    LogRegistry::storage()->info("[FSCache] Initialized with next inode: {}", nextInode_);
}

void FSCache::initRoot() {
    const auto rootEntry = FSEntryQueries::getRootEntry();
    if (!rootEntry) {
        LogRegistry::storage()->error("[FSCache] Root entry not found in the database");
        throw std::runtime_error("Root entry not found in the database");
    }

    if (!rootEntry->inode) {
        rootEntry->inode = std::make_optional(getOrAssignInode("/"));
        FSEntryQueries::updateFSEntry(rootEntry);
    } else if (rootEntry->inode != FUSE_ROOT_ID) {
        LogRegistry::storage()->warn("[FSCache] Root entry inode is not FUSE_ROOT_ID, resetting to 1");
        rootEntry->inode = FUSE_ROOT_ID;
        FSEntryQueries::updateFSEntry(rootEntry);
    }

    cacheEntry(rootEntry);
}

void FSCache::restoreCache() {
    const auto rootEntry = getEntry("/");
    if (!rootEntry) throw std::runtime_error("[FSCache] Root entry not found, cannot restore cache");

    for (const auto& entry : FSEntryQueries::listDir(rootEntry->id, true)) {
        if (!entry) {
            LogRegistry::storage()->warn("[FSCache] Null entry found in directory listing for root");
            continue;
        }

        if (entry->inode) cacheEntry(entry);
        else {
            entry->inode = std::make_optional(getOrAssignInode(entry->fuse_path));
            FSEntryQueries::updateFSEntry(entry);
            cacheEntry(entry);
        }
    }
}

std::shared_ptr<FSEntry> FSCache::getEntry(const fs::path& absPath) {
    const auto path = makeAbsolute(absPath);
    LogRegistry::storage()->debug("[FSCache] Retrieving entry for path: {}", path.string());
    const auto it = pathToEntry_.find(path);
    if (it != pathToEntry_.end()) return it->second;

    try {
        const auto ino = getOrAssignInode(path);
        auto entry = FSEntryQueries::getFSEntryByInode(ino);
        if (!entry) {
            LogRegistry::storage()->warn("[FSCache] No entry found for path: {}", path.string());
            return nullptr;
        }

        if (entry->inode) cacheEntry(entry);
        else {
            entry->inode = std::make_optional(getOrAssignInode(path));
            cacheEntry(entry);
        }
        return entry;
    } catch (const std::exception& e) {
        LogRegistry::storage()->error("[FSCache] Error retrieving entry for path {}: {}", path.string(), e.what());
        return nullptr;
    }
}

fuse_ino_t FSCache::resolveInode(const fs::path& absPath) {
    std::shared_lock lock(mutex_);
    auto it = pathToInode_.find(absPath);
    if (it != pathToInode_.end()) return it->second;

    const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(absPath);
    if (!entry) {
        LogRegistry::storage()->warn("[FSCache] No entry found for path: {}", absPath.string());
        return FUSE_ROOT_ID; // or some error code
    }

    fuse_ino_t ino = entry && entry->inode ? *entry->inode : getOrAssignInode(absPath);
    if (ino == FUSE_ROOT_ID) {
        LogRegistry::storage()->error("[FSCache] Failed to resolve inode for path: {}", absPath.string());
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
        LogRegistry::storage()->debug("[FSCache] Path {} already linked to inode {}", absPath.string(), ino);
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

    LogRegistry::fs()->info("[FSCache] Cached entry: {} with inode {}", entry->fuse_path.string(), *entry->inode);
}

bool FSCache::entryExists(const fs::path& absPath) const {
    std::shared_lock lock(mutex_);
    return pathToEntry_.contains(absPath);
}

std::shared_ptr<FSEntry> FSCache::getEntryFromInode(const fuse_ino_t ino) const {
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

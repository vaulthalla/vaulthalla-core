#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/FSEntryQueries.hpp"

#include <iostream>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::config;

StorageManager::StorageManager() {
    inodeToPath_[FUSE_ROOT_ID] = "/";
    pathToInode_["/"] = FUSE_ROOT_ID;
}

void StorageManager::initStorageEngines() {
    std::scoped_lock lock(mutex_);

    engines_.clear();

    try {
        for (auto& vault : VaultQueries::listVaults()) {
            std::shared_ptr<StorageEngine> engine;
            if (vault->type == VaultType::Local) engine = std::make_shared<StorageEngine>(vault);
            else if (vault->type == VaultType::S3) {
                const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);
                engine = std::make_shared<CloudStorageEngine>(s3Vault);
            }
            engines_[engine->root] = engine;
        }
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error initializing storage engines: " << e.what() << std::endl;
        throw;
    }
}

std::vector<std::shared_ptr<FSEntry>> StorageManager::listDir(const fs::path& absPath, const bool recursive) const {
    return FSEntryQueries::listDir(absPath, recursive);
}

std::shared_ptr<StorageEngine> StorageManager::resolveStorageEngine(const fs::path& absPath) const {
    std::scoped_lock lock(mutex_);

    std::shared_ptr<StorageEngine> bestMatch = nullptr;
    size_t bestMatchLen = 0;

    for (const auto& [rootStr, engine] : engines_) {
        const fs::path& root = engine->root;

        // Must be a prefix of absPath (and a path boundary, not just a substring)
        if (absPath == root || (absPath.native().starts_with(root.native()) &&
            (absPath.native().size() == root.native().size() || absPath.native()[root.native().size()] == fs::path::preferred_separator))) {

            size_t matchLen = root.native().size();
            if (matchLen > bestMatchLen) {
                bestMatchLen = matchLen;
                bestMatch = engine;
            }
            }
    }

    return bestMatch;
}

fuse_ino_t StorageManager::resolveInode(const fs::path& absPath) {
    std::shared_lock lock(inodeMutex_);
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

fuse_ino_t StorageManager::assignInode(const fs::path& path) {
    std::unique_lock lock(inodeMutex_);
    const auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) return it->second;

    const fuse_ino_t ino = nextInode_++;
    inodeToPath_[ino] = path;
    pathToInode_[path] = ino;
    return ino;
}

fuse_ino_t StorageManager::getOrAssignInode(const fs::path& path) {
    std::scoped_lock lock(inodeMutex_);
    auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) return it->second;

    fuse_ino_t new_ino = nextInode_++;
    pathToInode_[path] = new_ino;
    inodeToPath_[new_ino] = path;
    return new_ino;
}

void StorageManager::linkPath(const fs::path& absPath, const fuse_ino_t ino) {
    std::unique_lock lock(inodeMutex_);
    if (pathToInode_.find(absPath) != pathToInode_.end()) {
        std::cerr << "[StorageManager] Path already linked: " << absPath << std::endl;
        return; // Path already exists
    }
    pathToInode_[absPath] = ino;
    inodeToPath_[ino] = absPath;
}

fs::path StorageManager::resolvePathFromInode(const fuse_ino_t ino) const {
    std::shared_lock lock(inodeMutex_);
    const auto it = inodeToPath_.find(ino);
    if (it == inodeToPath_.end()) fs::path();
    return it->second;
}

void StorageManager::decrementInodeRef(const fuse_ino_t ino, const uint64_t nlookup) {
    std::unique_lock lock(inodeMutex_);
    auto it = inodeToPath_.find(ino);
    if (it == inodeToPath_.end()) return; // No such inode

    if (nlookup > 1) {
        // Decrement the reference count
        // This is a no-op, but could be extended
        return;
    }

    // If nlookup is 1, we can remove the inode
    pathToInode_.erase(it->second);
    inodeToPath_.erase(it);
}

std::vector<std::shared_ptr<StorageEngine>> StorageManager::getEngines() const {
    std::scoped_lock lock(mutex_);
    std::vector<std::shared_ptr<StorageEngine>> engines;
    engines.reserve(engines_.size());
    for (const auto& [_, engine] : engines_) engines.push_back(engine);
    return engines;
}

char StorageManager::getPathType(const fs::path& absPath) const {
    if (absPath.empty()) return 'd';
    const auto engine = resolveStorageEngine(absPath);
    if (!engine) {
        std::cerr << "[StorageManager] No storage engine found for path: " << absPath << std::endl;
        return 'u'; // unknown type
    }

    if (engine->isFile(engine->getRelativePath(absPath))) return 'f';
    if (engine->isDirectory(engine->getRelativePath(absPath))) return 'd';
    return 'u'; // unknown type
}

std::shared_ptr<FSEntry> StorageManager::getEntry(const fs::path& absPath) {
    const auto it = pathToEntry_.find(absPath);
    if (it != pathToEntry_.end()) return it->second;

    try {
        auto entry = FSEntryQueries::getFSEntry(absPath);
        if (!entry) {
            std::cerr << "[StorageManager] Entry not found for path: " << absPath << std::endl;
            return nullptr;
        }

        if (entry->inode) cacheEntry(*entry->inode, entry);
        else {
            entry->inode = std::make_optional(getOrAssignInode(absPath));
            cacheEntry(*entry->inode, entry);
        }
        return entry;
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error retrieving entry for path " << absPath << ": " << e.what() << std::endl;
        return nullptr;
    }
}

void StorageManager::cacheEntry(fuse_ino_t ino, std::shared_ptr<FSEntry> entry) {
    if (!entry) {
        std::cerr << "[StorageManager] Attempted to cache a null entry for inode " << ino << std::endl;
        return;
    }

    std::unique_lock lock(inodeMutex_);
    inodeToPath_[ino] = entry->abs_path;
    pathToInode_[entry->abs_path] = ino;
    pathToEntry_[entry->abs_path] = entry;
    inodeToEntry_[ino] = std::move(entry);
}

bool StorageManager::entryExists(const fs::path& absPath) const {
    std::shared_lock lock(inodeMutex_);
    return pathToEntry_.contains(absPath) || pathToInode_.contains(absPath) || FSEntryQueries::exists(absPath);
}

std::shared_ptr<FSEntry> StorageManager::getEntryFromInode(fuse_ino_t ino) const {
    std::shared_lock lock(inodeMutex_);
    auto it = inodeToEntry_.find(ino);
    if (it != inodeToEntry_.end()) return it->second;
    return nullptr;
}

void StorageManager::evictEntry(fuse_ino_t ino) {
    std::unique_lock lock(inodeMutex_);
    inodeToEntry_.erase(ino);
}

void StorageManager::evictPath(const fs::path& path) {
    std::unique_lock lock(inodeMutex_);
    auto it = pathToInode_.find(path);
    if (it != pathToInode_.end()) {
        fuse_ino_t ino = it->second;
        inodeToPath_.erase(ino);
        inodeToEntry_.erase(ino);
        pathToInode_.erase(path);
        pathToEntry_.erase(path);
    }
}

void StorageManager::registerInode(fuse_ino_t ino, const fs::path& path, std::shared_ptr<FSEntry> entry) {
    std::unique_lock lock(inodeMutex_);
    inodeToPath_[ino] = path;
    pathToInode_[path] = ino;
    inodeToEntry_[ino] = std::move(entry);
}

void StorageManager::updateCachedEntry(const std::shared_ptr<FSEntry>& entry) {
    if (!entry) return;
    std::unique_lock lock(inodeMutex_);
    for (auto& [ino, cached] : inodeToEntry_) {
        if (cached && cached->id == entry->id) {
            inodeToEntry_[ino] = entry;
            break;
        }
    }
}

#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/FSEntry.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"

#include <iostream>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

StorageManager::StorageManager() {
    initStorageEngines();
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
    const auto engine = resolveStorageEngine(absPath);
    const auto relPath = absPath.lexically_relative(engine->root);
    return DirectoryQueries::listDir(engine->vault->id, relPath, recursive);
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

fs::path StorageManager::resolvePathFromInode(const fuse_ino_t ino) const {
    std::shared_lock lock(inodeMutex_);
    const auto it = inodeToPath_.find(ino);
    if (it == inodeToPath_.end())
        throw std::runtime_error("resolvePathFromInode: Unknown inode: " + std::to_string(ino));
    return it->second;
}

std::vector<std::shared_ptr<StorageEngine>> StorageManager::getEngines() const {
    std::scoped_lock lock(mutex_);
    std::vector<std::shared_ptr<StorageEngine>> engines;
    engines.reserve(engines_.size());
    for (const auto& [_, engine] : engines_) engines.push_back(engine);
    return engines;
}

#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "storage/VaultEncryptionManager.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"
#include "types/FSEntry.hpp"
#include "types/User.hpp"
#include "types/Path.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "util/fsPath.hpp"
#include "services/ThumbnailWorker.hpp"
#include "services/ServiceDepsRegistry.hpp"

#include <iostream>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::config;
using namespace vh::services;

StorageManager::StorageManager() {
    inodeToPath_[FUSE_ROOT_ID] = "/";
    pathToInode_["/"] = FUSE_ROOT_ID;
    cacheEntry(FSEntryQueries::getFSEntry("/"));
    nextInode_ = FSEntryQueries::getNextInode();
}

void StorageManager::initStorageEngines() {
    std::cout << "[StorageManager] Initializing storage engines..." << std::endl;
    std::scoped_lock lock(mutex_);

    engines_.clear();

    try {
        for (auto& vault : VaultQueries::listVaults()) {
            std::cout << "[StorageManager] Initializing engine for vault: " << vault->name << " (ID: " << vault->id << ")" << std::endl;
            std::shared_ptr<StorageEngine> engine;
            if (vault->type == VaultType::Local) engine = std::make_shared<StorageEngine>(vault);
            else if (vault->type == VaultType::S3) {
                const auto s3Vault = std::static_pointer_cast<S3Vault>(vault);
                engine = std::make_shared<CloudStorageEngine>(s3Vault);
            }
            engines_[engine->paths->absRelToRoot(engine->paths->vaultRoot, PathType::FUSE_ROOT)] = engine;
            vaultToEngine_[vault->id] = engine;
        }
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error initializing storage engines: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<StorageEngine> StorageManager::resolveStorageEngine(const fs::path& absPath) const {
    std::scoped_lock lock(mutex_);

    fs::path current;
    for (const auto& part : absPath.lexically_normal()) {
        current /= part;

        // Remove trailing slash by converting to generic form and trimming
        std::string currentStr = current.generic_string();
        if (!currentStr.empty() && currentStr.back() == '/')
            currentStr.pop_back();

        auto it = engines_.find(currentStr);
        if (it != engines_.end()) {
            return it->second;
        }
    }

    std::cout << "[StorageManager] No storage engine found for path: " << absPath << std::endl;

    std::cout << "[StorageManager] Available engines: " << std::endl;
    for (const auto& [path, engine] : engines_) {
        std::cout << " - " << path << " (Vault ID: " << engine->vault->id << ", Type: " << to_string(engine->vault->type) << ")" << std::endl;
    }

    return nullptr;
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

fs::path StorageManager::resolvePathFromInode(fuse_ino_t ino) {
    std::shared_lock lock(inodeMutex_);
    auto it = inodeToPath_.find(ino);
    if (it == inodeToPath_.end()) throw std::runtime_error("No path for inode");
    return it->second;
}

void StorageManager::decrementInodeRef(const fuse_ino_t ino, const uint64_t nlookup) {
    std::unique_lock lock(inodeMutex_);
    auto it = inodeToPath_.find(ino);
    if (it == inodeToPath_.end()) return; // No such inode

    if (nlookup > 1) return;  // This is a no-op, but could be extended

    // If nlookup is 1, we can remove the inode
    pathToInode_.erase(it->second);
    inodeToPath_.erase(it);
    inodeToEntry_.erase(ino);
    pathToEntry_.erase(it->second);
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

    const auto relPath = engine->paths->relPath(absPath, PathType::VAULT_ROOT);
    if (engine->isFile(relPath)) return 'f';
    if (engine->isDirectory(relPath)) return 'd';
    return 'u'; // unknown type
}

std::shared_ptr<FSEntry> StorageManager::getEntry(const fs::path& absPath) {
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

void StorageManager::cacheEntry(const std::shared_ptr<FSEntry>& entry) {
    if (!entry || !entry->inode) throw std::invalid_argument("Entry or inode is null");

    std::unique_lock lock(inodeMutex_);
    inodeToPath_[*entry->inode] = entry->abs_path;
    inodeToEntry_[*entry->inode] = entry;
    pathToInode_[entry->abs_path] = *entry->inode;
    pathToEntry_[entry->abs_path] = entry;
}

bool StorageManager::entryExists(const fs::path& absPath) const {
    std::shared_lock lock(inodeMutex_);
    return pathToEntry_.contains(absPath) || FSEntryQueries::exists(absPath);
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

void StorageManager::queuePendingRename(fuse_ino_t ino, const fs::path& oldPath, const fs::path& newPath) {
    std::scoped_lock lock(renameQueueMutex_);
    renameRequests_[ino] = PendingRename{ oldPath, newPath };
}

void StorageManager::clearPendingRename(fuse_ino_t ino) {
    std::scoped_lock lock(renameQueueMutex_);
    renameRequests_.erase(ino);
}

std::optional<PendingRename> StorageManager::getPendingRename(fuse_ino_t ino) const {
    std::scoped_lock lock(renameQueueMutex_);
    const auto it = renameRequests_.find(ino);
    if (it != renameRequests_.end()) return it->second;
    return std::nullopt;
}

void StorageManager::initUserStorage(const std::shared_ptr<User>& user) {
    try {
        std::cout << "[StorageManager] Initializing storage for user: " << user->name << std::endl;

        if (!user->id) throw std::runtime_error("User ID is not set. Cannot initialize storage.");

        auto vault = std::make_shared<Vault>();
        vault->name = user->name + "'s Local Disk Vault";
        vault->description = "Default local disk vault for " + user->name;
        vault->mount_point = fs::path(config::ConfigRegistry::get().fuse.root_mount_path) / "users" / user->name;

        {
            std::lock_guard lock(mutex_);
            vault->id = VaultQueries::upsertVault(vault);
            vault = VaultQueries::getVault(vault->id);
        }

        if (!vault) throw std::runtime_error("Failed to create or retrieve vault for user: " + user->name);

        vaultToEngine_[vault->id] = std::make_shared<StorageEngine>(vault);

        std::cout << "[StorageManager] Initialized storage for user: " << user->name << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error initializing user storage: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<Vault> StorageManager::addVault(std::shared_ptr<Vault> vault,
                                                const std::shared_ptr<Sync>& sync) {
    if (!vault) throw std::invalid_argument("Vault cannot be null");
    std::lock_guard lock(mutex_);

    vault->id = VaultQueries::upsertVault(vault, sync);
    vault = VaultQueries::getVault(vault->id);
    vaultToEngine_[vault->id] = std::make_shared<StorageEngine>(vault);

    return vault;
}

void StorageManager::updateVault(const std::shared_ptr<Vault>& vault) {
    if (!vault) throw std::invalid_argument("Vault cannot be null");
    if (vault->id == 0) throw std::invalid_argument("Vault ID cannot be zero");
    std::lock_guard lock(mutex_);
    VaultQueries::upsertVault(vault);
    vaultToEngine_[vault->id]->vault = vault;
}

void StorageManager::removeVault(const unsigned int vaultId) {
    std::lock_guard lock(mutex_);
    VaultQueries::removeVault(vaultId);

    vaultToEngine_.erase(vaultId);
    std::cout << "[StorageManager] Removed vault with ID: " << vaultId << std::endl;
}

std::shared_ptr<Vault> StorageManager::getVault(const unsigned int vaultId) const {
    std::lock_guard lock(mutex_);
    if (vaultToEngine_.find(vaultId) != vaultToEngine_.end()) return vaultToEngine_.at(vaultId)->vault;
    return VaultQueries::getVault(vaultId);
}

std::shared_ptr<StorageEngine> StorageManager::getEngine(const unsigned int id) const {
    std::scoped_lock lock(mutex_);
    if (!vaultToEngine_.contains(id))
        throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(id));
    return vaultToEngine_.at(id);
}

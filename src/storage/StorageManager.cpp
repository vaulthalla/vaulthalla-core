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
#include "services/ServiceDepsRegistry.hpp"

#include <iostream>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::config;
using namespace vh::services;

StorageManager::StorageManager() = default;

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

void StorageManager::queuePendingRename(fuse_ino_t ino, const fs::path& oldPath, const fs::path& newPath, std::optional<unsigned int> userId) {
    std::scoped_lock lock(renameQueueMutex_);
    renameRequests_[ino] = PendingRename{ oldPath, newPath, userId };
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

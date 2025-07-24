#include "storage/StorageManager.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "types/LocalDiskVault.hpp"
#include "types/S3Vault.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "storage/LocalDiskStorageEngine.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/FSEntry.hpp"
#include "types/Directory.hpp"
#include "util/Magic.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "database/Queries/SyncQueries.hpp"
#include "concurrency/thumbnail/ThumbnailWorker.hpp"
#include "services/SyncController.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

StorageManager::StorageManager()
    : thumbnailWorker_(std::make_shared<concurrency::ThumbnailWorker>()) {
    initStorageEngines();
}

void StorageManager::initStorageEngines() {
    std::lock_guard lock(mountsMutex_);
    try {
        for (auto& vault : VaultQueries::listVaults()) {
            const auto sync = SyncQueries::getSync(vault->id);
            if (vault->type == VaultType::Local) {
                auto localVault = std::static_pointer_cast<LocalDiskVault>(vault);
                if (!localVault) throw std::runtime_error("Failed to cast vault to LocalDiskVault");
                engines_[vault->id] = std::make_shared<LocalDiskStorageEngine>(thumbnailWorker_, sync, localVault);
            } else if (vault->type == VaultType::S3) {
                auto vaultS3 = std::static_pointer_cast<S3Vault>(vault);
                if (!vaultS3) throw std::runtime_error("Failed to cast vault to S3Vault");
                const auto key = APIKeyQueries::getAPIKey(vaultS3->api_key_id);
                engines_[vault->id] = std::make_shared<CloudStorageEngine>(thumbnailWorker_, vaultS3, key, sync);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error initializing storage engines: " << e.what() << std::endl;
        throw;
    }
}

void StorageManager::initializeControllers() {
    syncController_ = std::make_shared<services::SyncController>(shared_from_this());
    syncController_->start();
}

void StorageManager::initUserStorage(const std::shared_ptr<User>& user) {
    try {
        std::cout << "[StorageManager] Initializing storage for user: " << user->name << std::endl;

        if (!user->id) throw std::runtime_error("User ID is not set. Cannot initialize storage.");

        std::shared_ptr<Vault> vault =
            std::make_shared<LocalDiskVault>(user->name + "'s Local Disk Vault",
                                             std::filesystem::path(
                                                 config::ConfigRegistry::get().fuse.root_mount_path) /
                                             "users" / user->name); {
            std::lock_guard lock(mountsMutex_);
            vault->id = VaultQueries::addVault(vault);
            vault = VaultQueries::getVault(vault->id);
        }

        if (!vault) throw std::runtime_error("Failed to create or retrieve vault for user: " + user->name);

        engines_[vault->id] = std::make_shared<LocalDiskStorageEngine>(
            thumbnailWorker_,
            SyncQueries::getSync(vault->id),
            std::static_pointer_cast<LocalDiskVault>(vault));

        std::cout << "[StorageManager] Initialized storage for user: " << user->name << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error initializing user storage: " << e.what() << std::endl;
        throw;
    }
}

std::shared_ptr<Vault> StorageManager::addVault(std::shared_ptr<Vault> vault,
                                                const std::shared_ptr<Sync>& sync) {
    if (!vault) throw std::invalid_argument("Vault cannot be null");
    std::lock_guard lock(mountsMutex_);

    try {
        vault->id = VaultQueries::addVault(vault, sync);
        vault = VaultQueries::getVault(vault->id);
        if (vault->type == VaultType::Local) {
            auto localVault = std::static_pointer_cast<LocalDiskVault>(vault);
            if (!localVault) throw std::runtime_error("Failed to cast vault to LocalDiskVault");
            engines_[vault->id] = std::make_shared<LocalDiskStorageEngine>(thumbnailWorker_, sync, localVault);
        } else if (vault->type == VaultType::S3) {
            auto vaultS3 = std::static_pointer_cast<S3Vault>(vault);
            if (!vaultS3) throw std::runtime_error("Failed to cast vault to S3Vault");
            const auto key = APIKeyQueries::getAPIKey(vaultS3->api_key_id);
            const auto engine = std::make_shared<CloudStorageEngine>(thumbnailWorker_, vaultS3, key, sync);
            engines_[vault->id] = engine;
        }

        return vault;
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error adding vault: " << e.what() << std::endl;
        throw;
    }
}

void StorageManager::removeVault(const unsigned int vaultId) {
    std::lock_guard lock(mountsMutex_);
    VaultQueries::removeVault(vaultId);

    engines_.erase(vaultId);
    std::cout << "[StorageManager] Removed vault with ID: " << vaultId << std::endl;
}

std::vector<std::shared_ptr<Vault> > StorageManager::listVaults(const std::shared_ptr<User>& user) const {
    std::lock_guard lock(mountsMutex_);
    if (user->isAdmin()) return VaultQueries::listVaults();
    return VaultQueries::listUserVaults(user->id);
}

std::shared_ptr<Vault> StorageManager::getVault(const unsigned int vaultId) const {
    std::lock_guard lock(mountsMutex_);
    if (engines_.find(vaultId) != engines_.end()) return engines_.at(vaultId)->getVault();
    return VaultQueries::getVault(vaultId);
}

void StorageManager::finishUpload(const unsigned int vaultId, const unsigned int userId,
                                  const std::filesystem::path& relPath) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->finishUpload(userId, relPath);
}

void StorageManager::removeEntry(const unsigned int userId, const unsigned int vaultId, const std::filesystem::path& relPath) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->remove(relPath, userId);
    syncNow(vaultId);
}

void StorageManager::mkdir(const unsigned int vaultId, const std::string& relPath,
                           const std::shared_ptr<User>& user) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->mkdir(relPath, user->id);
}

void StorageManager::move(const unsigned int vaultId, const unsigned int userId,
                          const std::filesystem::path& from, const std::filesystem::path& to) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->move(from, to, userId);
}

void StorageManager::rename(const unsigned int vaultId, const unsigned int userId,
                            const std::string& from, const std::string& to) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->rename(from, to, userId);
}

void StorageManager::copy(const unsigned int vaultId, const unsigned int userId,
                          const std::filesystem::path& from, const std::filesystem::path& to) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->copy(from, to, userId);
}

std::vector<std::shared_ptr<FSEntry> > StorageManager::listDir(const unsigned int vaultId,
                                                               const std::string& relPath,
                                                               const bool recursive) const {
    std::lock_guard lock(mountsMutex_);
    return DirectoryQueries::listDir(vaultId, relPath, recursive);
}

void StorageManager::syncNow(const unsigned int vaultId) const {
    syncController_->runNow(vaultId);
}

std::vector<std::shared_ptr<StorageEngine> > StorageManager::getEngines() const {
    std::lock_guard lock(mountsMutex_);
    std::vector<std::shared_ptr<StorageEngine>> result;
    result.reserve(engines_.size());
    std::ranges::transform(engines_.begin(), engines_.end(),
                           std::back_inserter(result),
                           [](const auto& pair) { return pair.second; });
    return result;
}

std::shared_ptr<StorageEngine> StorageManager::getEngine(const unsigned int id) const {
    if (auto localEngine = getEngine<LocalDiskStorageEngine>(id)) return localEngine;
    if (auto cloudEngine = getEngine<CloudStorageEngine>(id)) return cloudEngine;
    return nullptr;
}

bool StorageManager::pathsAreConflicting(const std::filesystem::path& path1, const std::filesystem::path& path2) {
    std::error_code ec;

    auto weak1 = std::filesystem::weakly_canonical(path1, ec);
    if (ec) weak1 = std::filesystem::absolute(path1); // fallback if partial canonical fails

    ec.clear();
    auto weak2 = std::filesystem::weakly_canonical(path2, ec);
    if (ec) weak2 = std::filesystem::absolute(path2);

    return weak1 == weak2;
}

bool StorageManager::hasLogicalParent(const std::filesystem::path& relPath) {
    if (relPath.empty() || relPath == "/") return false; // Root path has no parent
    return relPath.has_parent_path() && !relPath.parent_path().empty() && relPath.parent_path() != "/";
}
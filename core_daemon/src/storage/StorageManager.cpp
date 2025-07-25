#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/FSEntry.hpp"
#include "util/Magic.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "protocols/FUSECmdClient.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

namespace fs = std::filesystem;

StorageManager::StorageManager() {
    initStorageEngines();
}

void StorageManager::initStorageEngines() {
    std::lock_guard lock(mountsMutex_);
    for (auto& vault : VaultQueries::listVaults())
        engines_[vault->id] = std::make_shared<StorageEngine>(vault);
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
            std::lock_guard lock(mountsMutex_);
            vault->id = VaultQueries::upsertVault(vault);
            vault = VaultQueries::getVault(vault->id);
        }

        if (!vault) throw std::runtime_error("Failed to create or retrieve vault for user: " + user->name);

        engines_[vault->id] = std::make_shared<StorageEngine>(vault);

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

    vault->id = VaultQueries::upsertVault(vault, sync);
    vault = VaultQueries::getVault(vault->id);
    engines_[vault->id] = std::make_shared<StorageEngine>(vault);

    return vault;
}

void StorageManager::updateVault(const std::shared_ptr<Vault>& vault) {
    if (!vault) throw std::invalid_argument("Vault cannot be null");
    if (vault->id == 0) throw std::invalid_argument("Vault ID cannot be zero");
    std::lock_guard lock(mountsMutex_);
    VaultQueries::upsertVault(vault);
    engines_[vault->id]->vault = vault;
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
    if (engines_.find(vaultId) != engines_.end()) return engines_.at(vaultId)->vault;
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
    syncNow(engine->vault->id);
}

void StorageManager::rename(const unsigned int vaultId, const unsigned int userId,
                            const std::string& from, const std::string& to) const {
    const auto engine = getEngine(vaultId);
    engine->rename(from, to, userId);
    syncNow(engine->vault->id);
}

void StorageManager::copy(const unsigned int vaultId, const unsigned int userId,
                          const std::filesystem::path& from, const std::filesystem::path& to) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));
    engine->copy(from, to, userId);
    syncNow(engine->vault->id);
}

std::vector<std::shared_ptr<FSEntry> > StorageManager::listDir(const unsigned int vaultId,
                                                               const std::string& relPath,
                                                               const bool recursive) const {
    std::lock_guard lock(mountsMutex_);
    return DirectoryQueries::listDir(vaultId, relPath, recursive);
}

std::shared_ptr<StorageEngine> StorageManager::getEngine(const unsigned int id) const {
    std::scoped_lock lock(mountsMutex_);
    if (!engines_.contains(id))
        throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(id));
    return engines_.at(id);
}

void StorageManager::syncNow(const unsigned int vaultId) { sendSyncCommand(vaultId); }

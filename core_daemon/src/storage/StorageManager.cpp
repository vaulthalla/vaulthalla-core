#include "storage/StorageManager.hpp"
#include "types/User.hpp"
#include "types/Vault.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/LocalDiskStorageEngine.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace vh::storage {

StorageManager::StorageManager() {
    initStorageEngines();
}

void StorageManager::initStorageEngines() {
    std::lock_guard lock(mountsMutex_);
    try {
        for (auto& vault : database::VaultQueries::listVaults()) {
            if (vault->type == types::VaultType::Local) {
                auto localVault = std::static_pointer_cast<types::LocalDiskVault>(vault);
                if (!localVault) throw std::runtime_error("Failed to cast vault to LocalDiskVault");
                engines_[vault->id] = std::make_shared<LocalDiskStorageEngine>(localVault);
            } else if (vault->type == types::VaultType::S3) {
                auto vaultS3 = std::static_pointer_cast<types::S3Vault>(vault);
                if (!vaultS3) throw std::runtime_error("Failed to cast vault to S3Vault");
                engines_[vault->id] = std::make_shared<CloudStorageEngine>(vaultS3);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error initializing storage engines: " << e.what() << "\n";
        throw;
    }
}

void StorageManager::initUserStorage(const std::shared_ptr<types::User>& user) {
    try {
        std::cout << "[StorageManager] Initializing storage for user: " << user->name << "\n";

        if (!user->id) throw std::runtime_error("User ID is not set. Cannot initialize storage.");

        std::shared_ptr<types::Vault> vault =
            std::make_shared<types::LocalDiskVault>(user->name + "'s Local Disk Vault",
                                                    std::filesystem::path(
                                                        config::ConfigRegistry::get().fuse.root_mount_path) /
                                                    "users" / user->name); {
            std::lock_guard lock(mountsMutex_);
            vault->id = database::VaultQueries::addVault(vault);
            vault = database::VaultQueries::getVault(vault->id);
        }

        if (!vault) throw std::runtime_error("Failed to create or retrieve vault for user: " + user->name);

        engines_[vault->id] = std::make_shared<LocalDiskStorageEngine>(
            std::static_pointer_cast<types::LocalDiskVault>(vault));

        std::cout << "[StorageManager] Initialized storage for user: " << user->name << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error initializing user storage: " << e.what() << "\n";
        throw;
    }
}

void StorageManager::addVault(std::shared_ptr<types::Vault>&& vault) {
    if (!vault) throw std::invalid_argument("Vault cannot be null");
    std::lock_guard lock(mountsMutex_);

    database::VaultQueries::addVault(vault);
    vault = database::VaultQueries::getVault(vault->id);
    if (vault->type == types::VaultType::Local) {
        auto localVault = std::static_pointer_cast<types::LocalDiskVault>(vault);
        if (!localVault) throw std::runtime_error("Failed to cast vault to LocalDiskVault");
        engines_[vault->id] = std::make_shared<LocalDiskStorageEngine>(localVault);
    } else if (vault->type == types::VaultType::S3) {
        auto vaultS3 = std::static_pointer_cast<types::S3Vault>(vault);
        if (!vaultS3) throw std::runtime_error("Failed to cast vault to S3Vault");
        engines_[vault->id] = std::make_shared<CloudStorageEngine>(vaultS3);
    }
}

void StorageManager::removeVault(const unsigned int vaultId) {
    std::lock_guard lock(mountsMutex_);
    database::VaultQueries::removeVault(vaultId);

    engines_.erase(vaultId);
    std::cout << "[StorageManager] Removed vault with ID: " << vaultId << "\n";
}

std::vector<std::shared_ptr<types::Vault> > StorageManager::listVaults(const std::shared_ptr<types::User>& user) const {
    std::lock_guard lock(mountsMutex_);
    if (user->isAdmin()) return database::VaultQueries::listVaults();
    return database::VaultQueries::listUserVaults(user->id);
}

std::shared_ptr<types::Vault> StorageManager::getVault(unsigned int vaultId) const {
    std::lock_guard lock(mountsMutex_);
    if (engines_.find(vaultId) != engines_.end()) return engines_.at(vaultId)->getVault();
    return database::VaultQueries::getVault(vaultId);
}

void StorageManager::finishUpload(const unsigned int vaultId,
                                  const std::filesystem::path& relPath,
                                  const std::shared_ptr<types::User>& user) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));

    const auto f = std::make_shared<types::File>();

    if (engine->type() == StorageType::Local) {
        const auto localEngine = std::static_pointer_cast<LocalDiskStorageEngine>(engine);
        const auto absPath = localEngine->getAbsolutePath(relPath);
        if (!std::filesystem::exists(absPath))
            throw std::runtime_error(
                "File does not exist at path: " + absPath.string());
        if (!std::filesystem::is_regular_file(absPath))
            throw std::runtime_error(
                "Path is not a regular file: " + absPath.string());

        f->vault_id = vaultId;
        f->name = relPath.filename().string();
        f->size_bytes = std::filesystem::file_size(absPath);
        f->created_by = user->id;
        f->path = absPath.string();

        if (!relPath.has_parent_path() || relPath.parent_path().string() == "/") f->parent_id = std::nullopt;
        else f->parent_id = database::FileQueries::getFileIdByPath(relPath.parent_path());
    } {
        std::lock_guard lock(mountsMutex_);
        database::FileQueries::addFile(f);
    }

    std::cout << "[StorageManager] Finished upload for vault ID: " << vaultId << ", path: " << relPath << "\n";
}

void StorageManager::mkdir(const unsigned int vaultId, const std::string& relPath,
                           const std::shared_ptr<types::User>& user) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));

    if (engine->type() == StorageType::Local) {
        const auto localEngine = std::static_pointer_cast<LocalDiskStorageEngine>(engine);
        const auto absPath = localEngine->getAbsolutePath(std::filesystem::path(relPath));
        localEngine->mkdir(relPath);

        const auto d = std::make_shared<types::Directory>();

        d->vault_id = vaultId;
        d->name = std::filesystem::path(relPath).filename().string();
        d->created_by = user->id;
        d->last_modified_by = user->id;
        d->path = absPath.string();
        if (!hasLogicalParent(relPath)) d->parent_id = std::nullopt;
        else d->parent_id = database::FileQueries::getFileIdByPath(std::filesystem::path(relPath).parent_path());

        std::lock_guard lock(mountsMutex_);
        database::FileQueries::addDirectory(d);
    } else {
        throw std::runtime_error("Unsupported storage engine type for mkdir operation");
    }
}

std::vector<std::shared_ptr<types::FSEntry> > StorageManager::listDir(const unsigned int vaultId,
                                                                      const std::string& relPath,
                                                                      const bool recursive) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));

    if (engine->type() == StorageType::Local) {
        const auto localEngine = std::static_pointer_cast<LocalDiskStorageEngine>(engine);
        const auto absPath = localEngine->getAbsolutePath(std::filesystem::path(relPath));
        std::lock_guard lock(mountsMutex_);
        return database::FileQueries::listDir(vaultId, absPath, recursive);
    }

    return {};
}

std::shared_ptr<LocalDiskStorageEngine> StorageManager::getLocalEngine(const unsigned int id) const {
    std::lock_guard lock(mountsMutex_);
    const auto it = engines_.find(id);
    if (it != engines_.end()) return std::static_pointer_cast<LocalDiskStorageEngine>(it->second);
    return nullptr;
}

std::shared_ptr<CloudStorageEngine> StorageManager::getCloudEngine(const unsigned int id) const {
    std::lock_guard lock(mountsMutex_);
    const auto it = engines_.find(id);
    if (it != engines_.end()) return std::static_pointer_cast<CloudStorageEngine>(it->second);
    return nullptr;
}

std::shared_ptr<StorageEngine> StorageManager::getEngine(const unsigned int id) const {
    if (auto localEngine = getLocalEngine(id)) return localEngine;
    if (auto cloudEngine = getCloudEngine(id)) return cloudEngine;
    return nullptr; // No engine found for the given ID
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

} // namespace vh::storage
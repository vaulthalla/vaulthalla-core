#include "storage/StorageManager.hpp"
#include "types/db/Volume.hpp"
#include "types/db/User.hpp"
#include "types/db/Vault.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/LocalDiskStorageEngine.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/config/ConfigRegistry.hpp"
#include "types/db/File.hpp"

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
            const auto volumes = database::VaultQueries::listVaultVolumes(vault->id);
            if (vault->type == types::VaultType::Local) {
                auto localVault = std::static_pointer_cast<types::LocalDiskVault>(vault);
                if (!localVault) throw std::runtime_error("Failed to cast vault to LocalDiskVault");
                engines_[vault->id] = std::make_shared<LocalDiskStorageEngine>(localVault, volumes);
            } else if (vault->type == types::VaultType::S3) {
                auto vaultS3 = std::static_pointer_cast<types::S3Vault>(vault);
                if (!vaultS3) throw std::runtime_error("Failed to cast vault to S3Vault");
                engines_[vault->id] = std::make_shared<CloudStorageEngine>(vaultS3, volumes);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[StorageManager] Error initializing storage engines: " << e.what() << "\n";
        throw;
    }
}

void StorageManager::initUserStorage(const std::shared_ptr<types::User>& user) {
    try {
        std::cout << "[StorageManager] Initializing storage for user: " << user->email << "\n";

        if (!user->id) throw std::runtime_error("User ID is not set. Cannot initialize storage.");

        std::shared_ptr<types::Vault> vault =
            std::make_shared<types::LocalDiskVault>(user->name + "'s Local Disk Vault",
                                                    std::filesystem::path(
                                                        types::config::ConfigRegistry::get().fuse.root_mount_path) /
                                                    "users" / user->email); {
            std::lock_guard lock(mountsMutex_);
            vault->id = database::VaultQueries::addVault(vault);
            vault = database::VaultQueries::getVault(vault->id);
        }

        if (!vault) throw std::runtime_error("Failed to create or retrieve vault for user: " + user->email);

        auto volume =
            std::make_shared<types::Volume>(vault->id, user->name + " Default Volume",
                                            std::filesystem::path(user->email + "_default_volume")); {
            std::lock_guard lock(mountsMutex_);
            volume->id = database::VaultQueries::addVolume(user->id, volume);
            volume = database::VaultQueries::getVolume(volume->id);
        }

        if (!volume) throw std::runtime_error("Failed to initialize user storage: Volume not found after creation");

        engines_[vault->id] = std::make_shared<LocalDiskStorageEngine>(
            std::static_pointer_cast<types::LocalDiskVault>(vault),
            database::VaultQueries::listVaultVolumes(vault->id));

        std::cout << "[StorageManager] Initialized storage for user: " << user->email << "\n";
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
        engines_[vault->id] = std::make_shared<LocalDiskStorageEngine>(
            localVault, database::VaultQueries::listVaultVolumes(vault->id));
    } else if (vault->type == types::VaultType::S3) {
        auto vaultS3 = std::static_pointer_cast<types::S3Vault>(vault);
        if (!vaultS3) throw std::runtime_error("Failed to cast vault to S3Vault");
        engines_[vault->id] = std::make_shared<CloudStorageEngine>(
            vaultS3, database::VaultQueries::listVaultVolumes(vault->id));
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

void StorageManager::finishUpload(const unsigned int vaultId, const unsigned int volumeId,
                                  const std::filesystem::path& relPath,
                                  const std::shared_ptr<types::User>& user) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));

    const auto f = std::make_shared<types::File>();

    if (engine->type() == StorageType::Local) {
        const auto localEngine = std::static_pointer_cast<LocalDiskStorageEngine>(engine);
        const auto absPath = localEngine->getAbsolutePath(relPath, volumeId);
        if (!std::filesystem::exists(absPath))
            throw std::runtime_error(
                "File does not exist at path: " + absPath.string());
        if (!std::filesystem::is_regular_file(absPath))
            throw std::runtime_error(
                "Path is not a regular file: " + absPath.string());

        const auto file = std::filesystem::status(absPath);

        f->storage_volume_id = volumeId;
        f->name = relPath.filename().string();
        f->is_directory = false;
        f->mode = static_cast<unsigned long long>(file.permissions());
        f->current_version_size_bytes = std::filesystem::file_size(absPath);
        f->uid = user->id;
        f->gid = 1; // TODO: handle group ownership
        f->created_by = user->id;
        f->full_path = absPath.string();

        if (!relPath.has_parent_path() || relPath.parent_path().string() == "/") f->parent_id = std::nullopt;
        else f->parent_id = database::FileQueries::getFileIdByPath(relPath.parent_path());
    } {
        std::lock_guard lock(mountsMutex_);
        database::FileQueries::addFile(f);
    }

    std::cout << "[StorageManager] Finished upload for vault ID: " << vaultId << ", volume ID: " << volumeId <<
        ", path: " << relPath << "\n";
}

std::vector<std::shared_ptr<types::File> > StorageManager::listDir(const unsigned int vaultId,
                                                                   const unsigned int volumeId,
                                                                   const std::string& relPath,
                                                                   const bool recursive) const {
    const auto engine = getEngine(vaultId);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vaultId));

    if (engine->type() == StorageType::Local) {
        const auto localEngine = std::static_pointer_cast<LocalDiskStorageEngine>(engine);
        const auto absPath = localEngine->getAbsolutePath(std::filesystem::path(relPath), volumeId);
        std::lock_guard lock(mountsMutex_);
        return database::FileQueries::listFilesInDir(volumeId, absPath, recursive);
    }

    return {};
}

void StorageManager::mountVolume(const std::shared_ptr<types::Volume>& volume) {
    if (!volume) throw std::invalid_argument("Volume cannot be null");
    std::lock_guard lock(mountsMutex_);
    const auto engine = getEngine(volume->vault_id);
    if (!engine)
        throw std::runtime_error(
            "No storage engine found for volume with ID: " + std::to_string(volume->vault_id));
    engine->mountVolume(volume);
    std::cout << "[StorageManager] Mounted volume: " << volume->name << " with ID: " << volume->id << "\n";
}

void StorageManager::addVolume(std::shared_ptr<types::Volume> volume, const unsigned int userId) {
    if (!volume) throw std::invalid_argument("Volume cannot be null"); {
        std::lock_guard lock(mountsMutex_);
        volume->id = database::VaultQueries::addVolume(userId, volume);
        volume = database::VaultQueries::getVolume(volume->id);
    }

    mountVolume(volume);
    std::cout << "[StorageManager] Added volume: " << volume->name << " with ID: " << volume->id << "\n";
}

void StorageManager::removeVolume(const unsigned int volumeId, unsigned int userId) {
    std::lock_guard lock(mountsMutex_);

    const auto volume = database::VaultQueries::getVolume(volumeId);
    if (!volume) throw std::runtime_error("Volume not found with ID: " + std::to_string(volumeId));

    // TODO: Check if user has access to this volume

    database::VaultQueries::removeVolume(volumeId);

    const auto engine = getEngine(volume->vault_id);
    engine->unmountVolume(volume);

    std::cout << "[StorageManager] Removed volume with ID: " << volumeId << "\n";
}

std::shared_ptr<types::Volume> StorageManager::getVolume(const unsigned int volumeId, unsigned int userId) const {
    std::lock_guard lock(mountsMutex_);
    auto volume = database::VaultQueries::getVolume(volumeId);
    if (!volume) throw std::runtime_error("Volume not found with ID: " + std::to_string(volumeId));

    // TODO: Check if user has access to this volume

    return volume;
}

std::vector<std::shared_ptr<types::Volume> > StorageManager::listVolumes(const unsigned int userId) const {
    std::lock_guard lock(mountsMutex_);
    return database::VaultQueries::listUserVolumes(userId);
}

std::shared_ptr<LocalDiskStorageEngine> StorageManager::getLocalEngine(const unsigned int id) const {
    std::lock_guard lock(mountsMutex_);
    auto it = engines_.find(id);
    if (it != engines_.end()) return std::static_pointer_cast<LocalDiskStorageEngine>(it->second);
    return nullptr;
}

std::shared_ptr<CloudStorageEngine> StorageManager::getCloudEngine(const unsigned int id) const {
    std::lock_guard lock(mountsMutex_);
    auto it = engines_.find(id);
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

} // namespace vh::storage
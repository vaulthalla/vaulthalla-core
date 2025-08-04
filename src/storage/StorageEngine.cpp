#include "storage/StorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/Operation.hpp"
#include "types/Path.hpp"
#include "util/Magic.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/SyncQueries.hpp"
#include "storage/VaultEncryptionManager.hpp"
#include "storage/Filesystem.hpp"
#include "util/files.hpp"
#include "services/ThumbnailWorker.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::types;
using namespace vh::database;
using namespace vh::encryption;
using namespace vh::config;
using namespace vh::storage;
using namespace vh::services;
using namespace vh::logging;
namespace fs = std::filesystem;

namespace vh::storage {

StorageEngine::StorageEngine(const std::shared_ptr<Vault>& vault)
    : vault(vault),
      sync(SyncQueries::getSync(vault->id)),
      paths(std::make_shared<Path>(vault->mount_point)),
      encryptionManager(std::make_shared<VaultEncryptionManager>(paths->vaultRoot)) {
    if (!FSEntryQueries::exists(paths->vaultRoot)) Filesystem::mkVault(paths->absRelToRoot(paths->vaultRoot, PathType::FUSE_ROOT), vault->id);
    if (!fs::exists(paths->cacheRoot)) fs::create_directories(paths->cacheRoot);
}

bool StorageEngine::isDirectory(const fs::path& rel_path) const {
    return DirectoryQueries::isDirectory(vault->id, rel_path);
}

bool StorageEngine::isFile(const fs::path& rel_path) const {
    return FileQueries::isFile(vault->id, rel_path);
}

std::vector<uint8_t> StorageEngine::decrypt(const unsigned int vaultId, const fs::path& relPath, const std::vector<uint8_t>& payload) const {
    const auto iv = FileQueries::getEncryptionIV(vaultId, relPath);
    if (!iv || iv->empty()) throw std::runtime_error("No encryption IV found for file: " + relPath.string());
    return encryptionManager->decrypt(payload, *iv);
}

uintmax_t StorageEngine::getDirectorySize(const fs::path& path) {
    uintmax_t total = 0;
    for (auto& p : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied))
        if (fs::is_regular_file(p.status())) total += fs::file_size(p);
    return total;
}

uintmax_t StorageEngine::getVaultSize() const { return getDirectorySize(paths->backingRoot); }
uintmax_t StorageEngine::getCacheSize() const { return getDirectorySize(paths->cacheRoot); }
uintmax_t StorageEngine::getVaultAndCacheTotalSize() const { return getVaultSize() + getCacheSize(); }
uintmax_t StorageEngine::freeSpace() const {
    return vault->quota - getVaultAndCacheTotalSize() - MIN_FREE_SPACE;
}

void StorageEngine::purgeThumbnails(const fs::path& rel_path) const {
    for (const auto& size : ConfigRegistry::get().caching.thumbnails.sizes) {
        const auto thumbnailPath = paths->absPath(rel_path, PathType::THUMBNAIL_ROOT) / std::to_string(size);
        if (fs::exists(thumbnailPath)) fs::remove(thumbnailPath);
    }
}

void StorageEngine::moveThumbnails(const std::filesystem::path& from, const std::filesystem::path& to) const {
    for (const auto& size : ConfigRegistry::get().caching.thumbnails.sizes) {
        auto fromPath = paths->absPath(from, PathType::THUMBNAIL_ROOT) / std::to_string(size);
        auto toPath = paths->absPath(to, PathType::THUMBNAIL_ROOT) / std::to_string(size);

        if (fromPath.extension() != ".jpg" && fromPath.extension() != ".jpeg") {
            fromPath += ".jpg";
            toPath += ".jpg";
        }

        if (!fs::exists(fromPath)) {
            LogRegistry::storage()->warn("[StorageEngine] Thumbnail does not exist: {}", fromPath.string());
            continue;
        }

        Filesystem::mkdir(toPath.parent_path());
        fs::rename(fromPath, toPath); // TODO: Handle rename properly
    }
}

void StorageEngine::copyThumbnails(const std::filesystem::path& from, const std::filesystem::path& to) const {
    for (const auto& size : ConfigRegistry::get().caching.thumbnails.sizes) {
        auto fromPath = paths->absPath(from, PathType::THUMBNAIL_ROOT) / std::to_string(size);
        auto toPath = paths->absPath(to, PathType::THUMBNAIL_ROOT) / std::to_string(size);

        if (fromPath.extension() != ".jpg" && fromPath.extension() != ".jpeg") {
            fromPath += ".jpg";
            toPath += ".jpg";
        }

        if (!fs::exists(fromPath)) {
            LogRegistry::storage()->warn("[StorageEngine] Thumbnail does not exist: {}", fromPath.string());
            continue;
        }

        Filesystem::mkdir(toPath.parent_path());
        fs::copy_file(fromPath, toPath, fs::copy_options::overwrite_existing);  // TODO: Handle copy properly
    }
}

void StorageEngine::mkdir(const fs::path& relPath, const unsigned int userId) {
    Filesystem::mkdir(paths->absRelToAbsRel(relPath, PathType::VAULT_ROOT, PathType::FUSE_ROOT), 0755, userId, shared_from_this());
}

void StorageEngine::move(const fs::path& from, const fs::path& to, const unsigned int userId) {
    Filesystem::rename(paths->absRelToAbsRel(from, PathType::VAULT_ROOT, PathType::FUSE_ROOT),
                                  paths->absRelToAbsRel(to, PathType::VAULT_ROOT, PathType::FUSE_ROOT), userId, shared_from_this());
}

void StorageEngine::rename(const fs::path& from, const fs::path& to, const unsigned int userId) {
    Filesystem::rename(paths->absRelToAbsRel(from, PathType::VAULT_ROOT, PathType::FUSE_ROOT),
                                  paths->absRelToAbsRel(to, PathType::VAULT_ROOT, PathType::FUSE_ROOT), userId, shared_from_this());
}

void StorageEngine::copy(const fs::path& from, const fs::path& to, const unsigned int userId) {
    Filesystem::copy(paths->absRelToAbsRel(from, PathType::VAULT_ROOT, PathType::FUSE_ROOT),
                     paths->absRelToAbsRel(to, PathType::VAULT_ROOT, PathType::FUSE_ROOT), userId, shared_from_this());
}

void StorageEngine::remove(const fs::path& rel_path, const unsigned int userId) {
    Filesystem::remove(paths->absRelToAbsRel(rel_path, PathType::VAULT_ROOT, PathType::FUSE_ROOT), userId, shared_from_this());
}

}
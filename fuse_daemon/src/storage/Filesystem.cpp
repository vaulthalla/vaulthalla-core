#include "storage/Filesystem.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Vault.hpp"
#include "types/Directory.hpp"
#include "types/File.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "util/fsPath.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "engine/VaultEncryptionManager.hpp"
#include "util/files.hpp"

#include <iostream>
#include <ranges>
#include <vector>
#include <algorithm>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::config;

void Filesystem::init(std::shared_ptr<StorageManager> manager) {
    std::lock_guard lock(mutex_);
    storageManager_ = std::move(manager);
}

bool Filesystem::isReady() {
    std::lock_guard lock(mutex_);
    return static_cast<bool>(storageManager_);
}

void Filesystem::mkdir(const fs::path& absPath, mode_t mode) {
    std::scoped_lock lock(mutex_);
    if (!storageManager_) throw std::runtime_error("StorageManager is not initialized");

    try {
        if (absPath.empty()) throw std::runtime_error("Cannot create directory at empty path");

        std::vector<fs::path> toCreate;
        fs::path cur = absPath;

        while (!cur.empty() && !storageManager_->entryExists(cur)) {
            toCreate.push_back(cur);
            cur = cur.parent_path();
        }

        std::ranges::reverse(toCreate);

        std::cout << "[Filesystem] Directories to create: " << toCreate.size() << std::endl;
        for (const auto& p : toCreate) {
            const auto path = makeAbsolute(p);

            if (FSEntryQueries::getEntryIdByPath(path)) continue;

            auto dir = std::make_shared<Directory>();

            if (const auto engine = storageManager_->resolveStorageEngine(path)) {
                dir->vault_id = engine->vault->id;
                dir->path = engine->resolveAbsolutePathToVaultPath(path);
            }

            dir->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(path));
            dir->abs_path = path;
            dir->backing_path = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(path);
            dir->name = path.filename();
            dir->created_at = dir->updated_at = std::time(nullptr);
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = storageManager_->assignInode(path);
            dir->is_hidden = false;
            dir->is_system = false;

            storageManager_->cacheEntry(dir);
            DirectoryQueries::upsertDirectory(dir);
        }

        std::cout << "[Filesystem] Successfully created directory: " << absPath.string() << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "[Filesystem] mkdir exception: " << ex.what() << std::endl;
    }
}

void Filesystem::mkVault(const fs::path& absPath, unsigned int vaultId, mode_t mode) {
    std::scoped_lock lock(mutex_);
    if (!storageManager_) throw std::runtime_error("StorageManager is not initialized");

    try {
        if (absPath.empty()) throw std::runtime_error("Cannot create directory at empty path");

        std::vector<fs::path> toCreate;
        fs::path cur = absPath;

        while (!cur.empty() && !storageManager_->entryExists(cur)) {
            toCreate.push_back(cur);
            cur = cur.parent_path();
        }

        std::ranges::reverse(toCreate);

        for (unsigned int i = 0; i < toCreate.size(); ++i) {
            const auto& path = makeAbsolute(toCreate[i]);

            if (FSEntryQueries::getEntryIdByPath(path)) continue;

            auto dir = std::make_shared<Directory>();

            if (i == toCreate.size() - 1) {
                dir->vault_id = vaultId;
                dir->path = "/";
            } else dir->path = makeAbsolute(path);

            dir->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(path));
            dir->abs_path = makeAbsolute(path);
            dir->backing_path = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(path);
            dir->name = path.filename();
            dir->created_at = dir->updated_at = std::time(nullptr);
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = storageManager_->assignInode(path);
            dir->is_hidden = false;
            dir->is_system = false;

            storageManager_->cacheEntry(dir);
            DirectoryQueries::upsertDirectory(dir);

            std::cout << "[Filesystem] Directory created: " << path.string() << std::endl;
        }

        std::cout << "[Filesystem] Successfully created directory: " << absPath.string() << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "[Filesystem] mkdir exception: " << ex.what() << std::endl;
    }
}

void Filesystem::mkCache(const fs::path& absPath, mode_t mode) {
    std::scoped_lock lock(mutex_);
    if (!storageManager_) throw std::runtime_error("StorageManager is not initialized");

    try {
        if (absPath.empty()) throw std::runtime_error("Cannot create directory at empty path");

        std::vector<fs::path> toCreate;
        fs::path cur = absPath;

        while (!cur.empty() && !storageManager_->entryExists(cur)) {
            toCreate.push_back(cur);
            cur = cur.parent_path();
        }

        std::ranges::reverse(toCreate);

        for (const auto & p : toCreate) {
            const auto path = makeAbsolute(p);

            if (FSEntryQueries::getEntryIdByPath(path)) continue;

            auto dir = std::make_shared<Directory>();

            dir->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(path));
            dir->path = path;
            dir->abs_path = path;
            dir->backing_path = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(path);
            dir->name = path.filename();
            dir->created_at = dir->updated_at = std::time(nullptr);
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = storageManager_->assignInode(path);
            dir->is_hidden = false;
            dir->is_system = false;

            storageManager_->cacheEntry(dir);
            DirectoryQueries::upsertDirectory(dir);

            std::cout << "[Filesystem] Directory created: " << path.string() << std::endl;
        }

        std::cout << "[Filesystem] Successfully created directory: " << absPath.string() << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "[Filesystem] mkdir exception: " << ex.what() << std::endl;
    }
}

bool Filesystem::exists(const fs::path& absPath) {
    std::lock_guard lock(mutex_);
    if (!storageManager_) throw std::runtime_error("StorageManager is not initialized");

    try {
        return storageManager_->entryExists(absPath);
    } catch (const std::exception& ex) {
        std::cerr << "[Filesystem] exists exception: " << ex.what() << std::endl;
        return false;
    }
}

void Filesystem::rename(const fs::path& oldPath, const fs::path& newPath) {
    std::cout << "[Filesystem] Renaming path from " << oldPath << " to " << newPath << std::endl;

    const auto mntRelOldPath = makeAbsolute(oldPath.lexically_relative(ConfigRegistry::get().fuse.root_mount_path));
    const auto mntRelNewPath = makeAbsolute(newPath.lexically_relative(ConfigRegistry::get().fuse.root_mount_path));

    const auto entry = storageManager_->getEntry(mntRelOldPath);
    const auto engine = storageManager_->resolveStorageEngine(mntRelNewPath);
    if (!engine) {
        throw std::filesystem::filesystem_error(
            "[Filesystem] No storage engine found for DB-backed rename",
            mntRelOldPath,
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

    if (entry->isDirectory()) {
        auto children = storageManager_->listDir(mntRelOldPath, true);
        if (!children.empty()) {
            std::cerr << "[Filesystem] WARN: Directory rename does not update children entries yet" << std::endl;
        }
    }

    std::cout << "[Filesystem] Applying DB and cache updates: " << mntRelOldPath << " → " << mntRelNewPath << std::endl;

    if (!entry || !engine) {
        std::cerr << "[Filesystem] updatePaths failed: entry or engine missing." << std::endl;
        return;
    }

    entry->name = mntRelNewPath.filename();
    entry->path = engine->resolveAbsolutePathToVaultPath(mntRelNewPath);
    entry->abs_path = mntRelNewPath;
    entry->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(mntRelNewPath));

    std::string iv_b64;
    const auto buffer = util::readFileToVector(oldPath);
    const auto ciphertext = engine->encryptionManager->encrypt(buffer, iv_b64);
    util::writeFile(newPath, ciphertext);

    fs::remove(oldPath);

    if (entry->isDirectory()) DirectoryQueries::upsertDirectory(std::static_pointer_cast<Directory>(entry));
    else {
        const auto file = std::static_pointer_cast<File>(entry);
        file->encryption_iv = iv_b64;
        FileQueries::upsertFile(file);
    }

    storageManager_->evictPath(mntRelOldPath);
    storageManager_->evictPath(mntRelNewPath);
    storageManager_->cacheEntry(entry);

    std::cout << "[Filesystem] Rename completed successfully for path: " << mntRelOldPath << " → " << mntRelNewPath << std::endl;
}

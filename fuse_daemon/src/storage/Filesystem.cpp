#include "storage/Filesystem.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Vault.hpp"
#include "types/Directory.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "util/fsPath.hpp"

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
        for (const auto& path : toCreate) {
            auto dir = std::make_shared<Directory>();

            if (const auto engine = storageManager_->resolveStorageEngine(path)) {
                dir->vault_id = engine->vault->id;
                dir->path = engine->resolveAbsolutePathToVaultPath(path);
            }

            if (auto parentEntry = storageManager_->getEntry(resolveParent(path)))
                dir->parent_id = parentEntry->id;

            dir->abs_path = makeAbsolute(path);
            dir->name = path.filename();
            dir->created_at = dir->updated_at = std::time(nullptr);
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = storageManager_->assignInode(path);
            dir->is_hidden = false;
            dir->is_system = false;

            storageManager_->cacheEntry(*dir->inode, dir);
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
            const auto& path = toCreate[i];
            auto dir = std::make_shared<Directory>();

            if (auto parentEntry = storageManager_->getEntry(resolveParent(path)))
                dir->parent_id = parentEntry->id;

            if (i == toCreate.size() - 1) {
                dir->vault_id = vaultId;
                dir->path = "/";
            } else dir->path = path;

            dir->abs_path = makeAbsolute(path);
            dir->name = path.filename();
            dir->created_at = dir->updated_at = std::time(nullptr);
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = storageManager_->assignInode(path);
            dir->is_hidden = false;
            dir->is_system = false;

            storageManager_->cacheEntry(*dir->inode, dir);
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

        for (const auto & path : toCreate) {
            auto dir = std::make_shared<Directory>();

            if (auto parentEntry = storageManager_->getEntry(resolveParent(path)))
                dir->parent_id = parentEntry->id;

            dir->path = makeAbsolute(path);
            dir->abs_path = makeAbsolute(path);
            dir->name = path.filename();
            dir->created_at = dir->updated_at = std::time(nullptr);
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = storageManager_->assignInode(path);
            dir->is_hidden = false;
            dir->is_system = false;

            storageManager_->cacheEntry(*dir->inode, dir);
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

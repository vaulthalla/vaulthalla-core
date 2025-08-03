#include "storage/Filesystem.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Vault.hpp"
#include "types/Directory.hpp"
#include "types/File.hpp"
#include "types/Path.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "util/fsPath.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "storage/VaultEncryptionManager.hpp"
#include "util/files.hpp"
#include "crypto/Hash.hpp"
#include "services/ThumbnailWorker.hpp"
#include "util/Magic.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "database/Transactions.hpp"
#include "util/fsDBHelpers.hpp"

#include <iostream>
#include <ranges>
#include <vector>
#include <algorithm>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;
using namespace vh::config;
using namespace vh::services;

void Filesystem::init(std::shared_ptr<StorageManager> manager) {
    std::lock_guard lock(mutex_);
    storageManager_ = std::move(manager);
}

bool Filesystem::isReady() {
    std::lock_guard lock(mutex_);
    return static_cast<bool>(storageManager_);
}

void Filesystem::mkdir(const fs::path& absPath, mode_t mode, const std::optional<unsigned int>& userId,
                       std::shared_ptr<StorageEngine> engine) {
    std::cout << "[Filesystem] Creating directory at: " << absPath.string() << std::endl;
    std::scoped_lock lock(mutex_);
    if (!storageManager_) throw std::runtime_error("StorageManager is not initialized");

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    try {
        if (absPath.empty()) throw std::runtime_error("Cannot create directory at empty path");

        std::vector<fs::path> toCreate;
        fs::path cur = absPath;

        while (!cur.empty() && !cache->entryExists(cur)) {
            toCreate.push_back(cur);
            cur = cur.parent_path();
        }

        std::ranges::reverse(toCreate);

        if (!engine) engine = storageManager_->resolveStorageEngine(absPath);

        for (const auto& p : toCreate) {
            const auto path = makeAbsolute(p);

            auto dir = std::make_shared<Directory>();

            const auto fullDiskPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(path);

            if (engine) {
                dir->vault_id = engine->vault->id;
                dir->path = engine->paths->absRelToAbsOther(path, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
            } else dir->path = path;

            dir->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(path));
            dir->fuse_path = path;
            dir->name = path.filename();
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = cache->assignInode(path);
            dir->is_hidden = dir->name.front() == '.' && !dir->name.starts_with("..");
            dir->is_system = false;
            if (userId) dir->created_at = dir->updated_at = *userId;

            cache->cacheEntry(dir);
            DirectoryQueries::upsertDirectory(dir);

            try {
                std::filesystem::create_directories(fullDiskPath);
            } catch (const std::exception& e) {
                throw std::runtime_error(
                    "Failed to create directory on disk: " + fullDiskPath.string() + " - " + e.what());
            }
        }

        std::cout << "[Filesystem] Successfully created directory: " << absPath.string() << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "[Filesystem] mkdir exception: " << ex.what() << std::endl;
    }
}

void Filesystem::mkVault(const fs::path& absPath, unsigned int vaultId, mode_t mode) {
    std::cout << "[Filesystem] Creating vault directory at: " << absPath.string() << std::endl;
    std::scoped_lock lock(mutex_);
    if (!storageManager_) throw std::runtime_error("StorageManager is not initialized");

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    try {
        if (absPath.empty()) throw std::runtime_error("Cannot create directory at empty path");

        std::vector<fs::path> toCreate;
        fs::path cur = absPath;

        while (!cur.empty() && !cache->entryExists(cur)) {
            toCreate.push_back(cur);
            if (cur.string() == "/") break; // stop at root
            cur = cur.parent_path();
        }

        std::ranges::reverse(toCreate);

        for (unsigned int i = 0; i < toCreate.size(); ++i) {
            const auto& path = makeAbsolute(toCreate[i]);

            std::cout << "[Filesystem] Processing path: " << path.string() << std::endl;

            if (FSEntryQueries::getEntryIdByPath(path)) continue;

            auto dir = std::make_shared<Directory>();

            if (i == toCreate.size() - 1) {
                dir->vault_id = vaultId;
                dir->path = "/";
            } else dir->path = makeAbsolute(path);

            dir->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(path));
            dir->fuse_path = makeAbsolute(path);
            dir->name = path.filename();
            dir->created_at = dir->updated_at = std::time(nullptr);
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = cache->assignInode(path);
            dir->is_hidden = false;
            dir->is_system = false;

            cache->cacheEntry(dir);
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

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    try {
        if (absPath.empty()) throw std::runtime_error("Cannot create directory at empty path");

        std::vector<fs::path> toCreate;
        fs::path cur = absPath;

        while (!cur.empty() && !cache->entryExists(cur)) {
            toCreate.push_back(cur);
            cur = cur.parent_path();
        }

        std::ranges::reverse(toCreate);

        for (const auto& p : toCreate) {
            const auto path = makeAbsolute(p);

            if (FSEntryQueries::getEntryIdByPath(path)) continue;

            auto dir = std::make_shared<Directory>();

            dir->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(path));
            dir->path = path;
            dir->name = path.filename();
            dir->created_at = dir->updated_at = std::time(nullptr);
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = cache->assignInode(path);
            dir->is_hidden = false;
            dir->is_system = false;

            cache->cacheEntry(dir);
            DirectoryQueries::upsertDirectory(dir);

            std::cout << "[Filesystem] Directory created: " << path.string() << std::endl;
        }

        std::cout << "[Filesystem] Successfully created directory: " << absPath.string() << std::endl;
    } catch (const std::exception& ex) {
        std::cerr << "[Filesystem] mkdir exception: " << ex.what() << std::endl;
    }
}

bool Filesystem::exists(const fs::path& absPath) {
    try {
        return ServiceDepsRegistry::instance().fsCache->entryExists(absPath);
    } catch (const std::exception& ex) {
        std::cerr << "[Filesystem] exists exception: " << ex.what() << std::endl;
        return false;
    }
}

void Filesystem::copy(const fs::path& from, const fs::path& to, unsigned int userId,
                      std::shared_ptr<StorageEngine> engine) {
    if (from == to) return;

    if (!engine) engine = storageManager_->resolveStorageEngine(from);
    if (!engine) throw std::runtime_error("[Filesystem] No storage engine found for copy operation");

    const auto fromVaultPath = engine->paths->absRelToAbsOther(from, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
    const auto toVaultPath = engine->paths->absRelToAbsOther(to, PathType::FUSE_ROOT, PathType::VAULT_ROOT);

    const bool isFile = engine->isFile(fromVaultPath);
    if (!isFile && !engine->isDirectory(fromVaultPath)) throw std::runtime_error(
        "[StorageEngine] Path does not exist: " + fromVaultPath.string());

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    const auto entry = cache->getEntry(from);

    entry->id = 0;
    entry->path = toVaultPath;
    entry->fuse_path = to;
    entry->name = to.filename().string();
    entry->created_by = entry->last_modified_by = userId;
    entry->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(to));
    entry->inode = cache->getOrAssignInode(to);
    entry->is_hidden = entry->name.front() == '.' && !entry->name.starts_with("..");
    entry->is_system = false;

    if (isFile) FileQueries::upsertFile(std::make_shared<File>(*std::static_pointer_cast<File>(entry)));
    else DirectoryQueries::upsertDirectory(std::make_shared<Directory>(*std::static_pointer_cast<Directory>(entry)));
}

void Filesystem::remove(const fs::path& path, const unsigned int userId, std::shared_ptr<StorageEngine> engine) {
    if (!engine) engine = storageManager_->resolveStorageEngine(path);
    if (!engine) throw std::runtime_error("[Filesystem] No storage engine found for remove operation");

    const auto& cache = ServiceDepsRegistry::instance().fsCache;
    const auto vaultPath = engine->paths->absRelToAbsOther(path, PathType::FUSE_ROOT, PathType::VAULT_ROOT);

    if (engine->isFile(vaultPath)) {
        FileQueries::markFileAsTrashed(userId, engine->vault->id, vaultPath);
        cache->evictPath(path);
    } else if (engine->isDirectory(vaultPath))
        for (const auto& file : FileQueries::listFilesInDir(engine->vault->id, vaultPath, true)) {
            FileQueries::markFileAsTrashed(userId, file->id);
            cache->evictPath(file->fuse_path);
        }
    else throw std::runtime_error("[StorageEngine] Path does not exist: " + vaultPath.string());
}

std::shared_ptr<FSEntry> Filesystem::createFile(const fs::path& path, uid_t uid, gid_t gid, mode_t mode) {
    const auto engine = storageManager_->resolveStorageEngine(path);
    if (!engine) {
        std::cerr << "[Filesystem] No storage engine found for path: " << path << std::endl;
        return nullptr;
    }

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    std::cout << "[Filesystem] Creating file at path: " << path << std::endl;

    const auto fullDiskPath = engine->paths->absPath(path, PathType::FUSE_ROOT);
    const auto fullBackingPath = engine->paths->absPath(path, PathType::BACKING_ROOT);

    const auto f = std::make_shared<File>();
    f->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(path));
    f->vault_id = engine->vault->id;
    f->name = path.filename();
    f->path = engine->paths->absRelToRoot(fullDiskPath, PathType::VAULT_ROOT);
    f->fuse_path = path;
    f->mode = mode;
    f->owner_uid = uid;
    f->group_gid = gid;
    f->is_hidden = path.filename().string().starts_with('.');
    f->created_at = std::time(nullptr);
    f->updated_at = f->created_at;
    f->inode = std::make_optional(cache->getOrAssignInode(path));
    f->mime_type = inferMimeTypeFromPath(path.filename());
    f->size_bytes = 0;

    if (!fs::exists(fullDiskPath.parent_path())) fs::create_directories(fullDiskPath.parent_path());
    std::filesystem::create_directories(fullBackingPath.parent_path());
    std::ofstream(fullBackingPath).close(); // create empty file
    if (!std::filesystem::exists(fullBackingPath))
        std::cerr << "[Filesystem] Failed to create real file at: " << fullDiskPath << std::endl;

    FileQueries::upsertFile(f);
    cache->cacheEntry(f);

    return f;
}

std::shared_ptr<File> Filesystem::createFile(const NewFileContext& ctx) {
    const auto engine = ctx.engine ? ctx.engine : storageManager_->resolveStorageEngine(ctx.path);
    if (!engine) throw std::runtime_error("[Filesystem] No storage engine found for file creation");

    const auto& cache = ServiceDepsRegistry::instance().fsCache;
    std::cout << "[Filesystem] Creating file at path: " << ctx.path << std::endl;
    if (ctx.path.empty()) throw std::runtime_error("Cannot create file at empty path");
    if (cache->entryExists(ctx.fuse_path)) {
        std::cerr << "[Filesystem] File already exists at path: " << ctx.path << std::endl;
        const auto entry = cache->getEntry(ctx.fuse_path);
        if (entry->isDirectory()) throw std::filesystem::filesystem_error(
            "[Filesystem] Cannot create file at path, a directory already exists",
            ctx.fuse_path,
            std::make_error_code(std::errc::file_exists));
        return std::static_pointer_cast<File>(entry);
    }

    const auto fullDiskPath = engine->paths->absPath(ctx.fuse_path, PathType::FUSE_ROOT);
    const auto fullBackingPath = engine->paths->absPath(ctx.fuse_path, PathType::BACKING_ROOT);

    const auto f = std::make_shared<File>();
    f->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(ctx.path));
    f->vault_id = engine->vault->id;
    f->name = ctx.path.filename();
    f->path = engine->paths->absRelToRoot(fullDiskPath, PathType::VAULT_ROOT);
    f->fuse_path = ctx.fuse_path;
    f->mode = ctx.mode;
    f->owner_uid = ctx.linux_uid;
    f->group_gid = ctx.linux_gid;
    f->is_hidden = ctx.path.filename().string().starts_with('.');
    f->created_by = f->last_modified_by = ctx.userId;
    f->inode = std::make_optional(cache->getOrAssignInode(ctx.fuse_path));
    f->mime_type = ctx.buffer.empty() ? inferMimeTypeFromPath(ctx.path) : util::Magic::get_mime_type_from_buffer(ctx.buffer);
    f->size_bytes = ctx.buffer.size();

    if (!fs::exists(fullDiskPath.parent_path())) fs::create_directories(fullDiskPath.parent_path());
    std::filesystem::create_directories(fullBackingPath.parent_path());

    if (ctx.buffer.empty()) std::ofstream(fullBackingPath).close();
    else {
        std::string iv_b64;
        const auto ciphertext = engine->encryptionManager->encrypt(ctx.buffer, iv_b64);
        f->encryption_iv = iv_b64;
        util::writeFile(fullBackingPath, ciphertext);
    }

    f->content_hash = crypto::Hash::blake2b(fullBackingPath);

    if (!std::filesystem::exists(fullBackingPath))
        std::cerr << "[Filesystem] Failed to create real file at: " << fullDiskPath << std::endl;

    f->id = FileQueries::upsertFile(f);
    cache->cacheEntry(f);

    if (f->size_bytes > 0 && f->mime_type && isPreviewable(*f->mime_type))
        ThumbnailWorker::enqueue(engine, ctx.buffer, f);

    return f;
}


void Filesystem::rename(const fs::path& oldPath, const fs::path& newPath, const std::optional<unsigned int>& userId,
                        std::shared_ptr<StorageEngine> engine) {
    std::cout << "[Filesystem] Renaming path from " << oldPath << " to " << newPath << std::endl;

    const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(oldPath);
    if (!engine) engine = storageManager_->resolveStorageEngine(oldPath);
    if (!engine) {
        throw std::filesystem::filesystem_error(
            "[Filesystem] No storage engine found for DB-backed rename",
            oldPath,
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

    Transactions::exec("Filesystem::rename", [&](pqxx::work& txn) {
        std::vector<uint8_t> buffer;
        if (entry->isDirectory()) {
            for (const auto& item : FSEntryQueries::listDir(oldPath, true)) {
                handleRename({
                    .from = item->fuse_path,
                    .to = updateSubdirPath(oldPath, newPath, item->fuse_path),
                    .buffer = buffer,
                    .userId = userId,
                    .engine = engine,
                    .entry = item,
                    .txn = txn
                });
                buffer.clear();
            }
        }

        handleRename({
            .from = oldPath,
            .to = newPath,
            .buffer = buffer,
            .userId = userId,
            .engine = engine,
            .entry = entry,
            .txn = txn
        });

        txn.commit();

        std::filesystem::remove_all(engine->paths->absPath(oldPath, PathType::BACKING_ROOT));
    });

    std::cout << "[Filesystem] Successfully renamed " << oldPath << " to " << newPath << std::endl;
}

void Filesystem::handleRename(const RenameContext& context) {
    const auto& engine = context.engine;
    const auto& entry = context.entry;
    const auto& oldPath = context.from;
    const auto& newPath = context.to;
    const auto& userId = context.userId;
    auto buffer = context.buffer;
    auto& txn = context.txn;

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    const auto oldAbsPath = engine->paths->absPath(oldPath, PathType::BACKING_ROOT);
    const auto newAbsPath = engine->paths->absPath(newPath, PathType::BACKING_ROOT);
    const auto oldVaultPath = engine->paths->absRelToAbsOther(oldPath, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
    const auto newVaultPath = engine->paths->absRelToAbsOther(newPath, PathType::FUSE_ROOT, PathType::VAULT_ROOT);

    if (!std::filesystem::exists(oldAbsPath)) {
        throw std::filesystem::filesystem_error(
            "[Filesystem] Source path does not exist: " + oldAbsPath.string(),
            oldAbsPath,
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

    unsigned int id = 0;
    if (entry->id == 0) id = FSEntryQueries::getEntryIdByPath(entry->fuse_path).value_or(0);
    else id = entry->id;

    entry->id = id;
    entry->name = newPath.filename();
    entry->path = newVaultPath;
    entry->fuse_path = newPath;
    entry->parent_id = FSEntryQueries::getEntryIdByPath(resolveParent(newPath));
    if (userId) entry->last_modified_by = *userId;

    if (entry->isDirectory()) std::filesystem::create_directories(newAbsPath);
    else {
        std::filesystem::create_directories(newAbsPath.parent_path());
        const auto f = std::static_pointer_cast<File>(entry);

        if (canFastPath(entry, engine)) {
            std::cout << "[Filesystem] Fast‑path rename (ciphertext untouched): "
                      << oldAbsPath << " -> " << newAbsPath << std::endl;

            std::filesystem::rename(oldAbsPath, newAbsPath);
            updateFSEntry(txn, entry);

            cache->evictPath(oldPath);
            cache->evictPath(newPath);
            cache->cacheEntry(entry);
            return; // bail early, no reencrypt
        }

        if (!f->encryption_iv.empty()) {
            const auto tmp = util::decrypt_file_to_temp(engine->vault->id, oldVaultPath, engine);
            buffer = util::readFileToVector(tmp);
        } else buffer = util::readFileToVector(oldAbsPath);

        if (!buffer.empty()) {
            std::string iv_b64;
            const auto ciphertext = engine->encryptionManager->encrypt(buffer, iv_b64);
            if (ciphertext.empty()) throw std::runtime_error("Encryption failed for file: " + oldAbsPath.string());
            util::writeFile(newAbsPath, ciphertext);

            f->encryption_iv = iv_b64;
            f->size_bytes = std::filesystem::file_size(newAbsPath);
            f->mime_type = util::Magic::get_mime_type_from_buffer(buffer);
            f->content_hash = crypto::Hash::blake2b(newAbsPath);

            if (f->size_bytes > 0 && f->mime_type && isPreviewable(*f->mime_type))
                ThumbnailWorker::enqueue(engine, buffer, f);

            updateFile(txn, f);
        }
    }

    updateFSEntry(txn, entry);

    cache->evictPath(oldPath);
    cache->evictPath(newPath);
    cache->cacheEntry(entry);
}

bool Filesystem::canFastPath(const std::shared_ptr<FSEntry>& entry, const std::shared_ptr<StorageEngine>& engine) {
    if (entry->isDirectory()) return false;
    const auto file = std::static_pointer_cast<File>(entry);
    if (file->encryption_iv.empty()) return false;
    return file->vault_id == engine->vault->id;
}

bool Filesystem::isPreviewable(const std::string& mimeType) {
    return mimeType.starts_with("image") || mimeType.starts_with("application") || mimeType.contains("pdf");
}


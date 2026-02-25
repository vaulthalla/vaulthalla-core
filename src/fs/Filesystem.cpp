#include "fs/Filesystem.hpp"
#include "storage/Manager.hpp"
#include "storage/Engine.hpp"
#include "vault/model/Vault.hpp"
#include "fs/model/File.hpp"
#include "fs/model/Directory.hpp"
#include "fs/model/Path.hpp"
#include "database/queries/DirectoryQueries.hpp"
#include "database/queries/FileQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "database/queries/FSEntryQueries.hpp"
#include "vault/EncryptionManager.hpp"
#include "fs/ops/file.hpp"
#include "crypto/util/hash.hpp"
#include "preview/thumbnail/Worker.hpp"
#include "fs/metadata/Magic.hpp"
#include "runtime/Deps.hpp"
#include "database/Transactions.hpp"
#include "log/Registry.hpp"
#include "database/queries/VaultQueries.hpp"
#include "crypto/id/Generator.hpp"
#include "fs/cache/Registry.hpp"
#include "database/encoding/u8.hpp"

#include <ranges>
#include <fstream>
#include <vector>
#include <algorithm>
#include <paths.h>

using namespace vh::storage;
using namespace vh::vault::model;
using namespace vh::database;
using namespace vh::database::encoding;
using namespace vh::config;
using namespace vh::crypto;
using namespace vh::fs;
using namespace vh::fs::ops;
using namespace vh::fs::model;
using namespace vh::fs::metadata;

static void updateFile(pqxx::work& txn, const std::shared_ptr<File>& file) {
    const auto exists = txn.exec(pqxx::prepped{"fs_entry_exists_by_inode"}, file->inode).one_field().as<bool>();
    const auto sizeRes = txn.exec(pqxx::prepped{"get_file_size_by_inode"}, file->inode);
    const auto existingSize = sizeRes.empty() ? 0 : sizeRes.one_field().as<unsigned int>();

    pqxx::params p;
    p.append(file->id);
    p.append(file->size_bytes);
    p.append(file->mime_type);
    p.append(file->content_hash);
    p.append(file->encryption_iv);

    txn.exec(pqxx::prepped{"update_file_only"}, p);

    std::optional<unsigned int> parentId = file->parent_id;
    while (parentId) {
        pqxx::params stats_params{parentId, file->size_bytes - existingSize, exists ? 0 : 1, 0}; // Increment size_bytes and file_count
        txn.exec(pqxx::prepped{"update_dir_stats"}, stats_params);
        const auto res = txn.exec(pqxx::prepped{"get_fs_entry_parent_id"}, parentId);
        if (res.empty()) break;
        parentId = res.one_field().as<std::optional<unsigned int>>();
    }
}

static void updateFSEntry(pqxx::work& txn, const std::shared_ptr<Entry>& entry) {
    pqxx::params p;
    p.append(entry->inode);
    p.append(entry->vault_id);
    p.append(entry->parent_id);
    p.append(entry->name);
    p.append(entry->base32_alias);
    p.append(entry->last_modified_by);
    p.append(to_utf8_string(entry->path.u8string()));
    p.append(entry->mode);
    p.append(entry->owner_uid);
    p.append(entry->group_gid);
    p.append(entry->is_hidden);
    p.append(entry->is_system);

    txn.exec(pqxx::prepped{"update_fs_entry_by_inode"}, p);
}

void Filesystem::init(const std::shared_ptr<Manager>& manager) {
    std::lock_guard lock(mutex_);
    storageManager_ = std::move(manager);
}

bool Filesystem::isReady() {
    std::lock_guard lock(mutex_);
    return static_cast<bool>(storageManager_);
}

void Filesystem::mkdir(const std::filesystem::path& absPath, mode_t mode, const std::optional<unsigned int>& userId,
                       std::shared_ptr<Engine> engine) {
    log::Registry::fs()->debug("Creating directory at: {}", absPath.string());
    std::scoped_lock lock(mutex_);
    if (!storageManager_) throw std::runtime_error("StorageManager is not initialized");

    const auto& cache = runtime::Deps::get().fsCache;

    try {
        if (absPath.empty()) throw std::runtime_error("Cannot create directory at empty path");

        std::vector<std::filesystem::path> toCreate;
        std::filesystem::path cur = absPath;

        while (!cur.empty() && !cache->entryExists(cur)) {
            toCreate.push_back(cur);
            cur = cur.parent_path();
        }

        std::ranges::reverse(toCreate);

        if (!engine) engine = storageManager_->resolveStorageEngine(absPath);

        for (const auto& p : toCreate) {
            const auto path = makeAbsolute(p);
            log::Registry::fs()->debug("[Filesystem::mkdir] Creating directory at: {}", path.string());

            auto dir = std::make_shared<Directory>();

            if (engine) {
                dir->vault_id = engine->vault->id;
                dir->path = engine->paths->absRelToAbsRel(path, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
            } else dir->path = path;

            const auto parent = runtime::Deps::get().fsCache->getEntry(resolveParent(path));
            if (!parent) {
                log::Registry::fs()->error("[Filesystem::mkdir] Parent directory does not exist: {}", path.parent_path().string());
                throw std::filesystem::filesystem_error(
                    "Parent directory does not exist",
                    path.parent_path(),
                    std::make_error_code(std::errc::no_such_file_or_directory));
            }

            dir->parent_id = parent->id;
            dir->fuse_path = path;
            dir->name = path.filename();
            dir->base32_alias = id::Generator({ .namespace_token = dir->name }).generate();
            dir->backing_path = parent->backing_path / dir->base32_alias;
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = cache->assignInode(path);
            dir->is_hidden = dir->name.front() == '.' && !dir->name.starts_with("..");
            dir->is_system = false;
            dir->created_by = dir->last_modified_by = userId;

            dir->id = DirectoryQueries::upsertDirectory(dir);
            cache->cacheEntry(dir);

            try {
                std::filesystem::create_directories(dir->backing_path);
            } catch (const std::exception& e) {
                throw std::runtime_error(
                    "Failed to create directory on disk: " + dir->backing_path.string() + " - " + e.what());
            }
        }

        log::Registry::fs()->debug("Successfully created directory at: {}", absPath.string());
    } catch (const std::exception& ex) {
        log::Registry::fs()->error("Failed to create directory: {} - {}", absPath.string(), ex.what());
        throw std::runtime_error("Failed to create directory: " + absPath.string() + " - " + ex.what());
    }
}

void Filesystem::mkVault(const std::filesystem::path& absPath, unsigned int vaultId, mode_t mode) {
    if (absPath.empty()) throw std::runtime_error("Cannot create directory at empty path");
    log::Registry::fs()->debug("Creating vault directory at: {}", absPath.string());

    std::scoped_lock lock(mutex_);
    if (!storageManager_) throw std::runtime_error("StorageManager is not initialized");

    const auto vault = VaultQueries::getVault(vaultId);
    if (!vault) throw std::runtime_error("Vault with ID " + std::to_string(vaultId) + " does not exist");

    try {
        const auto dir = std::make_shared<Directory>();
        dir->vault_id = vaultId;
        dir->path = "/";
        dir->name = to_snake_case(vault->name);
        dir->parent_id = FSEntryQueries::getRootEntry()->id;
        dir->base32_alias = vault->mount_point;
        dir->backing_path = paths::getBackingPath() / dir->base32_alias;
        dir->fuse_path = absPath;
        dir->created_at = dir->updated_at = std::time(nullptr);
        dir->mode = mode;
        dir->owner_uid = getuid();
        dir->group_gid = getgid();
        dir->inode = runtime::Deps::get().fsCache->assignInode(absPath);
        dir->is_hidden = false;
        dir->is_system = false;

        const auto root = FSEntryQueries::getRootEntry();

        dir->id = DirectoryQueries::upsertDirectory(dir);

        runtime::Deps::get().fsCache->cacheEntry(dir);

        if (!std::filesystem::exists(dir->backing_path)) std::filesystem::create_directories(dir->backing_path);

        log::Registry::fs()->debug("Successfully created vault directory at: {}", absPath.string());
    } catch (const std::exception& ex) {
        log::Registry::fs()->error("Failed to create vault directory: {} - {}", absPath.string(), ex.what());
        throw std::runtime_error("Failed to create vault directory: " + absPath.string() + " - " + ex.what());
    }
}

bool Filesystem::exists(const std::filesystem::path& absPath) {
    try {
        return runtime::Deps::get().fsCache->entryExists(absPath);
    } catch (const std::exception& ex) {
        log::Registry::fs()->error("Error checking existence of path {}: {}", absPath.string(), ex.what());
        return false;
    }
}

void Filesystem::copy(const std::filesystem::path& from, const std::filesystem::path& to, unsigned int userId,
                      std::shared_ptr<Engine> engine) {
    if (from == to) return;

    if (!engine) engine = storageManager_->resolveStorageEngine(from);
    if (!engine) throw std::runtime_error("[Filesystem] No storage engine found for copy operation");

    const auto toEngine = storageManager_->resolveStorageEngine(to);
    if (!toEngine) throw std::runtime_error("[Filesystem] No storage engine found for destination of copy operation");
    if (toEngine->vault->id != engine->vault->id)
        throw std::runtime_error("[Filesystem] Cross-vault copy operations are not supported");

    const auto fromVaultPath = engine->paths->absRelToAbsRel(from, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
    const auto toVaultPath = engine->paths->absRelToAbsRel(to, PathType::FUSE_ROOT, PathType::VAULT_ROOT);

    const bool isFile = engine->isFile(fromVaultPath);
    if (!isFile && !engine->isDirectory(fromVaultPath))
        throw std::runtime_error("[StorageEngine] Path does not exist: " + fromVaultPath.string());

    const auto& cache = runtime::Deps::get().fsCache;

    const auto entry = cache->getEntry(from);

    const auto parent = runtime::Deps::get().fsCache->getEntry(resolveParent(to));

    entry->id = 0;
    entry->path = toVaultPath;
    entry->fuse_path = to;
    entry->name = to.filename().string();
    entry->base32_alias = id::Generator({ .namespace_token = entry->name }).generate();
    entry->backing_path = parent->backing_path / entry->base32_alias;
    entry->created_by = entry->last_modified_by = userId;
    entry->parent_id = parent->id;
    entry->inode = cache->getOrAssignInode(to);
    entry->is_hidden = entry->name.front() == '.' && !entry->name.starts_with("..");
    entry->is_system = false;

    if (isFile) FileQueries::upsertFile(std::make_shared<File>(*std::static_pointer_cast<File>(entry)));
    else DirectoryQueries::upsertDirectory(std::make_shared<Directory>(*std::static_pointer_cast<Directory>(entry)));
}

void Filesystem::remove(const std::filesystem::path& path, const unsigned int userId) {
    const auto& cache = runtime::Deps::get().fsCache;
    const auto entry = cache->getEntry(path);
    if (!entry) throw std::runtime_error("[Filesystem] Path does not exist in cache: " + path.string());
    if (!entry->vault_id) throw std::runtime_error("[Filesystem] Entry has no associated vault ID: " + path.string());

    // !!! DO NOT CALL THIS FUNCTION DIRECTLY FROM FUSE CALLBACKS - WEBSOCKET OR INTERNAL ONLY !!!
    // This function recursively marks files as trashed in the database and deletes their backing paths
    // which is incompatible with how FUSE expects unlink/rmdir to behave.

    if (entry->isDirectory())
        for (const auto& file : FileQueries::listFilesInDir(*entry->vault_id, entry->path, true)) {
            FileQueries::markFileAsTrashed(userId, file->id);
            cache->evictPath(file->fuse_path);
        }
    else {
        FileQueries::markFileAsTrashed(userId, *entry->vault_id, entry->path);
        cache->evictPath(path);
    }

    if (std::filesystem::exists(entry->backing_path)) std::filesystem::remove_all(entry->backing_path);
}

std::shared_ptr<Entry> Filesystem::createFile(const std::filesystem::path& path, uid_t uid, gid_t gid, mode_t mode) {
    const auto engine = storageManager_->resolveStorageEngine(path);
    if (!engine) {
        log::Registry::fs()->error("No storage engine found for file creation at path: {}", path.string());
        return nullptr;
    }

    const auto& cache = runtime::Deps::get().fsCache;

    log::Registry::fs()->debug("Creating file at path: {}", path.string());

    const auto parent = runtime::Deps::get().fsCache->getEntry(resolveParent(path));

    const auto f = std::make_shared<File>();
    f->parent_id = parent->id;
    f->vault_id = engine->vault->id;
    f->name = path.filename();
    f->path = engine->paths->absRelToAbsRel(path, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
    f->fuse_path = path;
    f->base32_alias = id::Generator({ .namespace_token = f->name }).generate();
    f->backing_path = parent->backing_path / f->base32_alias;
    f->mode = mode;
    f->owner_uid = uid;
    f->group_gid = gid;
    f->is_hidden = path.filename().string().starts_with('.');
    f->created_at = std::time(nullptr);
    f->updated_at = f->created_at;
    f->inode = std::make_optional(cache->getOrAssignInode(path));
    f->mime_type = inferMimeTypeFromPath(path.filename());
    f->size_bytes = 0;

    std::filesystem::create_directories(f->backing_path.parent_path());
    std::ofstream(f->backing_path).close(); // create empty file
    if (!std::filesystem::exists(f->backing_path))
        throw std::runtime_error("[Filesystem] Failed to create real file at: " + f->backing_path.string());

    f->content_hash = hash::blake2b(f->backing_path);
    f->id = FileQueries::upsertFile(f);
    cache->cacheEntry(f);

    log::Registry::fs()->debug("Successfully created file at path: {}", path.string());
    return f;
}

std::shared_ptr<File> Filesystem::createFile(const NewFileContext& ctx) {
    const auto engine = ctx.engine ? ctx.engine : storageManager_->resolveStorageEngine(ctx.path);
    if (!engine) throw std::runtime_error("[Filesystem] No storage engine found for file creation");

    log::Registry::fs()->debug("Creating file at path: {}, fuse_path: {}", ctx.path.string(), ctx.fuse_path.string());

    const auto& cache = runtime::Deps::get().fsCache;

    if (ctx.path.empty()) throw std::runtime_error("Cannot create file at empty path");
    if (cache->entryExists(ctx.fuse_path)) {
        if (!ctx.overwrite) {
            log::Registry::fs()->warn("File already exists at path: {}", ctx.fuse_path.string());
            const auto entry = cache->getEntry(ctx.fuse_path);
            if (entry->isDirectory()) throw std::filesystem::filesystem_error(
                "[Filesystem] Cannot create file at path, a directory already exists",
                ctx.fuse_path,
                std::make_error_code(std::errc::file_exists));
            return std::static_pointer_cast<File>(entry);
        }

        log::Registry::fs()->debug("File already exists at path: {}, overwriting", ctx.fuse_path.string());
        auto entry = cache->getEntry(ctx.fuse_path);
        if (entry->isDirectory()) {
            log::Registry::fs()->error("Cannot overwrite directory with file at path: {}", ctx.fuse_path.string());
            throw std::filesystem::filesystem_error(
                "[Filesystem] Cannot overwrite directory with file",
                ctx.fuse_path,
                std::make_error_code(std::errc::file_exists));
        }

        const auto f = std::static_pointer_cast<File>(entry);

        f->content_hash = hash::blake2b(entry->backing_path);

        if (!ctx.buffer.empty()) {
            const auto ciphertext = engine->encryptionManager->encrypt(ctx.buffer, f);
            writeFile(entry->backing_path, ciphertext);
            f->size_bytes = ctx.buffer.size();
            f->mime_type = Magic::get_mime_type_from_buffer(ctx.buffer);
        } else {
            f->size_bytes = std::filesystem::file_size(entry->backing_path);
            f->mime_type = inferMimeTypeFromPath(ctx.path);
        }

        FileQueries::updateFile(f);

        return f;
    }

    const auto parent = runtime::Deps::get().fsCache->getEntry(resolveParent(ctx.fuse_path));
    if (!parent) {
        log::Registry::fs()->error("Parent directory does not exist for path: {}", ctx.fuse_path.string());
        throw std::filesystem::filesystem_error(
            "[Filesystem] Parent directory does not exist",
            ctx.fuse_path,
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

    const auto f = std::make_shared<File>();
    f->parent_id = parent->id;
    f->vault_id = engine->vault->id;
    f->name = ctx.path.filename();
    f->path = ctx.path;
    f->fuse_path = ctx.fuse_path;
    f->base32_alias = id::Generator({ .namespace_token = f->name }).generate();
    f->backing_path = parent->backing_path / f->base32_alias;
    f->mode = ctx.mode;
    f->owner_uid = ctx.linux_uid;
    f->group_gid = ctx.linux_gid;
    f->is_hidden = ctx.path.filename().string().starts_with('.');
    f->created_by = f->last_modified_by = ctx.userId;
    f->inode = std::make_optional(cache->getOrAssignInode(ctx.fuse_path));
    f->mime_type = ctx.buffer.empty() ? inferMimeTypeFromPath(ctx.path) : Magic::get_mime_type_from_buffer(ctx.buffer);
    f->size_bytes = ctx.buffer.size();

    if (ctx.buffer.empty()) std::ofstream(f->backing_path).close();
    else {
        std::string iv_b64;
        const auto ciphertext = engine->encryptionManager->encrypt(ctx.buffer, f);
        writeFile(f->backing_path, ciphertext);
    }

    f->content_hash = hash::blake2b(f->backing_path);

    if (!std::filesystem::exists(f->backing_path))
        throw std::runtime_error("[Filesystem] Failed to create real file at: " + f->backing_path.string());

    f->id = FileQueries::upsertFile(f);
    cache->cacheEntry(f);

    if (f->size_bytes > 0 && f->mime_type && isPreviewable(*f->mime_type))
        preview::thumbnail::Worker::enqueue(engine, ctx.buffer, f);

    log::Registry::fs()->debug("Successfully created file at path: {}", ctx.path.string());
    return f;
}

void Filesystem::rename(const std::filesystem::path& oldPath, const std::filesystem::path& newPath, const std::optional<unsigned int>& userId,
                        std::shared_ptr<Engine> engine) {
    log::Registry::fs()->debug("Renaming {} to {}", oldPath.string(), newPath.string());

    const auto entry = runtime::Deps::get().fsCache->getEntry(oldPath);
    if (!engine) engine = storageManager_->resolveStorageEngine(oldPath);
    if (!engine) {
        throw std::filesystem::filesystem_error(
            "[Filesystem] No storage engine found for DB-backed rename",
            oldPath,
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

    const auto toEngine = storageManager_->resolveStorageEngine(newPath);
    if (!toEngine) throw std::runtime_error("[Filesystem] No storage engine found for destination of copy operation");
    if (toEngine->vault->id != engine->vault->id)
        throw std::runtime_error("[Filesystem] Cross-vault copy operations are not supported");

    const auto oldBackingPath = entry->backing_path;

    Transactions::exec("Filesystem::rename", [&](pqxx::work& txn) {
        std::vector<uint8_t> buffer;
        if (entry->isDirectory()) {
            if (!entry->parent_id) {
                log::Registry::fs()->error("Cannot rename root directory: {}", oldPath.string());
                throw std::runtime_error("[Filesystem] Cannot rename root directory");
            }

            for (const auto& item : runtime::Deps::get().fsCache->listDir(*entry->parent_id, true)) {
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

        if (entry->backing_path != oldBackingPath) std::filesystem::remove_all(oldBackingPath);
    });

    runtime::Deps::get().fsCache->evictPath(oldPath);
    runtime::Deps::get().fsCache->evictPath(newPath);
    runtime::Deps::get().fsCache->cacheEntry(entry);

    log::Registry::fs()->debug("Successfully renamed {} to {}", oldPath.string(), newPath.string());
}

void Filesystem::handleRename(const RenameContext& ctx) {
    const auto& cache = runtime::Deps::get().fsCache;

    const auto oldBackingPath = ctx.entry->backing_path;
    if (!std::filesystem::exists(oldBackingPath)) {
        throw std::filesystem::filesystem_error(
            "[Filesystem] Source path does not exist: " + oldBackingPath.string(),
            oldBackingPath,
            std::make_error_code(std::errc::no_such_file_or_directory));
    }

    const auto& entry = ctx.entry;

    unsigned int id = 0;
    if (entry->id == 0) {
        const auto e = runtime::Deps::get().fsCache->getEntry(entry->fuse_path);
        if (e) id = e->id;
        else id = 0;
    }
    else id = entry->id;

    const auto parent = runtime::Deps::get().fsCache->getEntry(resolveParent(ctx.to));
    if (!parent) throw std::runtime_error("Parent directory does not exist: " + resolveParent(ctx.to).string());

    const auto oldVaultPath = ctx.entry->path;

    entry->id = id;
    entry->name = ctx.to.filename();
    entry->path = ctx.engine->paths->absRelToAbsRel(ctx.to, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
    entry->fuse_path = ctx.to;
    entry->parent_id = parent->id;
    entry->created_by = entry->last_modified_by = ctx.userId;

    entry->backing_path = parent->backing_path / entry->base32_alias;

    if (entry->isDirectory()) std::filesystem::create_directories(entry->backing_path);
    else {
        std::filesystem::create_directories(entry->backing_path.parent_path());
        const auto f = std::static_pointer_cast<File>(entry);

        if (canFastPath(entry, ctx.engine)) {
            log::Registry::fs()->debug("Fast path rename for file: {}", ctx.from.string());

            std::filesystem::rename(oldBackingPath, entry->backing_path);
            updateFSEntry(ctx.txn, entry);

            cache->evictPath(ctx.from);
            cache->evictPath(ctx.to);
            cache->cacheEntry(entry);
            return; // bail early, no reencrypt
        }

        auto buffer = ctx.buffer;

        if (!f->encryption_iv.empty()) {
            const auto tmp = decrypt_file_to_temp(ctx.engine->vault->id, oldVaultPath, ctx.engine);
            buffer = readFileToVector(tmp);
        } else buffer = readFileToVector(oldBackingPath);

        if (buffer.empty()) {
            std::ofstream(entry->backing_path).close();
            if (!std::filesystem::exists(entry->backing_path))
                throw std::runtime_error("Failed to create real file at: " + entry->backing_path.string());
        } else {
            const auto ciphertext = ctx.engine->encryptionManager->encrypt(buffer, f);
            if (ciphertext.empty()) throw std::runtime_error("Encryption failed for file: " + oldBackingPath.string());
            writeFile(entry->backing_path, ciphertext);

            f->size_bytes = std::filesystem::file_size(entry->backing_path);
            f->mime_type = Magic::get_mime_type_from_buffer(buffer);
            f->content_hash = hash::blake2b(entry->backing_path);

            if (f->size_bytes > 0 && f->mime_type && isPreviewable(*f->mime_type))
                preview::thumbnail::Worker::enqueue(ctx.engine, buffer, f);

            updateFile(ctx.txn, f);
        }
    }

    updateFSEntry(ctx.txn, entry);

    cache->evictPath(ctx.from);
    cache->evictPath(ctx.to);
    cache->cacheEntry(entry);

    log::Registry::fs()->debug("Renamed {} to {}", ctx.from.string(), ctx.to.string());
}

bool Filesystem::canFastPath(const std::shared_ptr<Entry>& entry, const std::shared_ptr<Engine>& engine) {
    if (entry->isDirectory()) return false;
    const auto file = std::static_pointer_cast<File>(entry);
    if (file->encryption_iv.empty()) return false;
    return file->vault_id == engine->vault->id;
}

bool Filesystem::isPreviewable(const std::string& mimeType) {
    return mimeType.starts_with("image") || mimeType.starts_with("application") || mimeType.contains("pdf");
}

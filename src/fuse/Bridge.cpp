#include "fuse/Bridge.hpp"
#include "storage/Manager.hpp"
#include "storage/Engine.hpp"
#include "identities/model/User.hpp"
#include "fs/model/Path.hpp"
#include "fs/model/Entry.hpp"
#include "vault/model/Vault.hpp"
#include "config/ConfigRegistry.hpp"
#include "fs/Filesystem.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "logging/LogRegistry.hpp"
#include "fs/cache/Registry.hpp"

#include <cerrno>
#include <cstring>
#include <sys/statvfs.h>
#include <unistd.h>

using namespace vh::database;
using namespace vh::identities::model;
using namespace vh::storage;
using namespace vh::config;
using namespace vh::services;
using namespace vh::logging;
using namespace vh::fs;
using namespace vh::fs::model;

namespace vh::fuse {

void getattr(const fuse_req_t req, const fuse_ino_t ino, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[getattr] Called for inode: {}", ino);
    (void)fi;

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;
    // gid_t gid = ctx->gid;

    const auto user = UserQueries::getUserByLinuxUID(uid);
    if (!user) {
        LogRegistry::fuse()->error("[getattr] No user found for UID: {}", uid);
        fuse_reply_err(req, EACCES);
        return;
    }

    try {
        const auto& cache = ServiceDepsRegistry::instance().fsCache;
        if (ino == FUSE_ROOT_ID) {
            const auto entry = cache->getEntry(FUSE_ROOT_ID);
            if (!entry) {
                LogRegistry::fuse()->error("[getattr] No entry found for inode {}, resolved path: /", ino);
                fuse_reply_err(req, ENOENT);
                return;
            }

            struct stat st = statFromEntry(entry, ino);
            st.st_uid = getuid();  // explicitly set ownership
            st.st_gid = getgid();

            fuse_reply_attr(req, &st,  0.1);
            return;
        }

        const auto entry = cache->getEntry(ino);

        if (!entry) {
            LogRegistry::fuse()->error("[getattr] No entry found for inode {}", ino);
            fuse_reply_err(req, ENOENT);
            return;
        }

        if (!user->canManageVaults() && entry->vault_id && !user->canListVaultData(*entry->vault_id, entry->path)) {
            LogRegistry::fuse()->warn("[getattr] Access denied for user {} on root directory", user->name);
            fuse_reply_err(req, EACCES);
            return;
        }

        const auto st = statFromEntry(entry, ino);

        fuse_reply_attr(req, &st, 0.1); // match attr_timeout from lookup()
    } catch (const std::exception& ex) {
        fuse_reply_err(req, ENOENT);
    }
}

void setattr(const fuse_req_t req, const fuse_ino_t ino,
                         struct stat* attr, int to_set, fuse_file_info* fi) {
    (void)fi;
    const fuse_ctx* ctx = fuse_req_ctx(req);
    const uid_t uid = ctx->uid;

    const auto user = UserQueries::getUserByLinuxUID(uid);
    if (!user) {
        LogRegistry::fuse()->error("[setattr] No user found for UID: {}", uid);
        fuse_reply_err(req, EACCES);
        return;
    }

    LogRegistry::fuse()->debug("[setattr] Called for inode: {}, to_set: {}, uid: {}, gid: {}",
        ino, to_set, ctx->uid, ctx->gid);

    if (to_set & FUSE_SET_ATTR_MODE) {
        LogRegistry::fuse()->warn("⚔️ [Vaulthalla] Illegal access: chmod is forbidden beyond the gates!");
        fuse_reply_err(req, EPERM);
        return;
    }

    if (to_set & (FUSE_SET_ATTR_UID | FUSE_SET_ATTR_GID)) {
        LogRegistry::fuse()->warn("⚔️ [Vaulthalla] Illegal access: changing ownership is forbidden beyond the gates!");
        fuse_reply_err(req, EPERM);
        return;
    }

    try {
        const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(ino);
        if (!entry) {
            LogRegistry::fuse()->error("[setattr] No entry found for inode {}", ino);
            fuse_reply_err(req, ENOENT);
            return;
        }

        if (!user->canManageVaults() && entry->vault_id && !user->canCreateVaultData(*entry->vault_id, entry->path)) {
            LogRegistry::fuse()->warn("[setattr] Access denied for user {} on path {}", user->name, entry->path.string());
            fuse_reply_err(req, EACCES);
            return;
        }

        timespec times[2];
        if (to_set & FUSE_SET_ATTR_ATIME) times[0] = attr->st_atim;
        else times[0].tv_nsec = UTIME_OMIT;
        if (to_set & FUSE_SET_ATTR_MTIME) times[1] = attr->st_mtim;
        else times[1].tv_nsec = UTIME_OMIT;

        if (::utimensat(AT_FDCWD, entry->backing_path.c_str(), times, 0) < 0) {
            fuse_reply_err(req, errno);
            return;
        }

        // Re-stat file so kernel gets fresh info
        struct stat st = {};
        if (::stat(entry->backing_path.c_str(), &st) < 0) {
            fuse_reply_err(req, errno);
            return;
        }

        fuse_reply_attr(req, &st, 1.0);
    } catch (...) {
        fuse_reply_err(req, EIO);
    }
}

void readdir(const fuse_req_t req, const fuse_ino_t ino, const size_t size, const off_t off, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[readdir] Called for inode: {}, size: {}, offset: {}", ino, size, off);
    (void)fi;

    const fuse_ctx* ctx = fuse_req_ctx(req);
    const uid_t uid = ctx->uid;

    const auto listDirEntry = ServiceDepsRegistry::instance().fsCache->getEntry(ino);
    if (!listDirEntry) {
        LogRegistry::fuse()->error("[readdir] No entry found for inode {}", ino);
        fuse_reply_err(req, ENOENT);
        return;
    }

    const auto user = UserQueries::getUserByLinuxUID(uid);
    if (!user) {
        LogRegistry::fuse()->error("[readdir] No user found for UID: {}", uid);
        fuse_reply_err(req, EACCES);
        return;
    }

    if (!user->canManageVaults() && listDirEntry->vault_id && !user->canListVaultData(*listDirEntry->vault_id, listDirEntry->path)) {
        LogRegistry::fuse()->warn("[readdir] Access denied for user {} on path {}", user->name, listDirEntry->path.string());
        fuse_reply_err(req, EACCES);
        return;
    }

    const auto entries = ServiceDepsRegistry::instance().fsCache->listDir(listDirEntry->id, false);

    std::vector<char> buf(size);
    size_t buf_used = 0;

    auto add_entry = [&](const std::string& name, const struct stat& st, const off_t next_off) {
        const size_t entry_size = fuse_add_direntry(req, nullptr, 0, name.c_str(), &st, next_off);
        if (buf_used + entry_size > size) return false;

        fuse_add_direntry(req, buf.data() + buf_used, entry_size, name.c_str(), &st, next_off);
        buf_used += entry_size;
        return true;
    };

    off_t current_off = 0;

    if (off <= current_off++) {
        struct stat dot;
        dot.st_mode = S_IFDIR;
        if (!add_entry(".", dot, current_off)) goto reply;
    }

    if (off <= current_off++) {
        struct stat dotdot;
        dotdot.st_mode = S_IFDIR;
        if (!add_entry("..", dotdot, current_off)) goto reply;
    }

    for (size_t i = 0; i < entries.size(); ++i, ++current_off) {
        if (off > current_off) continue;

        const auto& entry = entries[i];
        const auto st = statFromEntry(entry, ino);

        if (!add_entry(entry->name, st, current_off + 1)) break;
    }

    reply:
        fuse_reply_buf(req, buf.data(), buf_used);
}

void lookup(const fuse_req_t req, const fuse_ino_t parent, const char* name) {
    LogRegistry::fuse()->debug("[lookup] Called for parent: {}, name: {}", parent, name);
    if (!name || strlen(name) == 0) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;
    // gid_t gid = ctx->gid;
    
    const auto user = UserQueries::getUserByLinuxUID(uid);
    if (!user) {
        LogRegistry::fuse()->debug("[lookup] No user found for UID: {}", uid);
        fuse_reply_err(req, EACCES);
        return;
    }

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    const auto parentPath = cache->resolvePath(parent);
    const auto path = parentPath / name;
    const fuse_ino_t ino = cache->getOrAssignInode(path);

    LogRegistry::fuse()->debug("[lookup] name: {}, parentPath: {}, inode: {}, Resolved path: {}", name, parentPath.string(), ino, path.string());

    const auto entry = cache->getEntry(path);
    if (!entry) {
        LogRegistry::fuse()->debug("[lookup] Entry not found for path: {}", path.string());
        fuse_reply_err(req, ENOENT);
        return;
    }

    if (!user->canManageVaults() && entry->vault_id && !user->canListVaultData(*entry->vault_id, entry->path)) {
        LogRegistry::fuse()->warn("[lookup] Access denied for user {} on path {}", user->name, path.string());
        fuse_reply_err(req, EACCES);
        return;
    }

    const auto st = statFromEntry(entry, ino);

    fuse_entry_param e{};
    e.ino = ino;
    e.attr_timeout = 0.1;
    e.entry_timeout = 0.1;
    e.attr = st;

    fuse_reply_entry(req, &e);
}

void create(const fuse_req_t req, const fuse_ino_t parent, const char* name, const mode_t mode, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[create] Called for parent: {}, name: {}, mode: {}",
        parent, name, mode);

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;

    if (!name || strlen(name) == 0) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    try {
        const auto parentPath = ServiceDepsRegistry::instance().fsCache->resolvePath(parent);
        const auto fullPath = parentPath / name;

        const auto engine = ServiceDepsRegistry::instance().storageManager->resolveStorageEngine(fullPath);
        if (!engine) {
            LogRegistry::fuse()->error("[create] No storage engine found for path: {}", fullPath.string());
            fuse_reply_err(req, EIO);
            return;
        }

        const auto vaultPath = engine->paths->absRelToAbsRel(fullPath, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
        const auto user = UserQueries::getUserByLinuxUID(uid);
        if (!user) {
            LogRegistry::fuse()->error("[create] No user found for UID: {}", uid);
            fuse_reply_err(req, EACCES);
            return;
        }

        if (!user->canManageVaults() && !user->canCreateVaultData(engine->vault->id, vaultPath)) {
            LogRegistry::fuse()->warn("[create] Access denied for user {} on path {}", user->name, fullPath.string());
            fuse_reply_err(req, EACCES);
            return;
        }

        if (ServiceDepsRegistry::instance().fsCache->entryExists(fullPath)) {
            fuse_reply_err(req, EEXIST);
            return;
        }

        const auto newEntry = Filesystem::createFile(fullPath, getuid(), getgid(), mode);
        const auto st       = statFromEntry(newEntry, *newEntry->inode);

        // open backing file immediately
        const int fd = ::open(newEntry->backing_path.c_str(), O_CREAT | O_RDWR, mode);
        if (fd < 0) {
            fuse_reply_err(req, errno);
            return;
        }

        auto* fh = new FileHandle{newEntry->backing_path.string(), fd};
        fi->fh = reinterpret_cast<uint64_t>(fh);

        fi->direct_io = 1;
        fi->keep_cache = 0;

        fuse_entry_param e{};
        e.ino           = *newEntry->inode;
        e.attr          = st;
        e.attr_timeout  = 60.0;
        e.entry_timeout = 60.0;

        fuse_reply_create(req, &e, fi);
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[create] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void open(const fuse_req_t req, const fuse_ino_t ino, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[open] Called for inode: {}, flags: {}", ino, fi->flags);

    ServiceDepsRegistry::instance().storageManager->registerOpenHandle(ino);

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;
    // gid_t gid = ctx->gid;

    const auto user = UserQueries::getUserByLinuxUID(uid);
    if (!user) {
        LogRegistry::fuse()->error("[open] No user found for UID: {}", uid);
        fuse_reply_err(req, EACCES);
        return;
    }

    try {
        const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(ino);
        if (!entry) {
            LogRegistry::fuse()->error("[open] No entry found for inode {}", ino);
            fuse_reply_err(req, ENOENT);
            return;
        }

        if (!user->canManageVaults() && entry->vault_id && !user->canDownloadVaultData(*entry->vault_id, entry->path)) {
            LogRegistry::fuse()->warn("[open] Access denied for user {} on path {}", user->name, entry->path.string());
            fuse_reply_err(req, EACCES);
            return;
        }

        const int fd = ::open(entry->backing_path.c_str(), fi->flags, 0644);
        if (fd < 0) {
            fuse_reply_err(req, errno);
            return;
        }

        auto* fh = new FileHandle{entry->backing_path.string(), fd};
        fi->fh = reinterpret_cast<uint64_t>(fh);

        fi->direct_io = 1;
        fi->keep_cache = 0;

        fuse_reply_open(req, fi);
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[open] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void write(const fuse_req_t req, const fuse_ino_t ino, const char* buf,
                       const size_t size, const off_t off, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[write] Called for inode: {}, size: {}, offset: {}, file handle: {}",
        ino, size, off, fi->fh);

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;

    const auto* fh = reinterpret_cast<FileHandle*>(fi->fh);
    (void)ino; // unused

    LogRegistry::fuse()->debug("[write] Writing to fd={} offset={} size={}", fh->fd, off, size);

    const ssize_t res = ::pwrite(fh->fd, buf, size, off);
    if (res < 0) fuse_reply_err(req, errno);

    const auto user = UserQueries::getUserByLinuxUID(uid);
    if (!user) {
        LogRegistry::fuse()->error("[write] No user found for UID: {}", uid);
        fuse_reply_err(req, EACCES);
        return;
    }

    const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(ino);
    if (!entry) {
        LogRegistry::fuse()->error("[write] No entry found for inode {}", ino);
        fuse_reply_err(req, ENOENT);
        return;
    }

    if (!user->canManageVaults() && entry->vault_id && !user->canCreateVaultData(*entry->vault_id, entry->path)) {
        LogRegistry::fuse()->warn("[write] Access denied for user {} on path {}", user->name, entry->path.string());
        fuse_reply_err(req, EACCES);
        return;
    }

    entry->size_bytes = std::filesystem::file_size(entry->backing_path);
    ServiceDepsRegistry::instance().fsCache->updateEntry(entry);

    fuse_lowlevel_notify_inval_inode(ServiceDepsRegistry::instance().fuseSession, ino, 0, 0);

    fuse_reply_write(req, res);
}

void read(const fuse_req_t req, const fuse_ino_t ino, const size_t size, const off_t off, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[read] Called for inode: {}, size: {}, offset: {}, file handle: {}",
        ino, size, off, fi->fh);

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;

    const auto* fh = reinterpret_cast<FileHandle*>(fi->fh);

    const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(ino);
    if (!entry) {
        LogRegistry::fuse()->error("[read] No entry found for inode {}", ino);
        fuse_reply_err(req, ENOENT);
        return;
    }

    const auto user = UserQueries::getUserByLinuxUID(uid);
    if (!user) {
        LogRegistry::fuse()->error("[read] No user found for UID: {}", uid);
        fuse_reply_err(req, EACCES);
        return;
    }

    if (!user->canManageVaults() && entry->vault_id && !user->canDownloadVaultData(*entry->vault_id, entry->path)) {
        LogRegistry::fuse()->warn("[read] Access denied for user {} on path {}", user->name, entry->path.string());
        fuse_reply_err(req, EACCES);
        return;
    }

    std::vector<char> buffer(size);
    const ssize_t res = ::pread(fh->fd, buffer.data(), size, off);

    if (res < 0) fuse_reply_err(req, errno);
    else fuse_reply_buf(req, buffer.data(), res);
}

void mkdir(const fuse_req_t req, const fuse_ino_t parent, const char* name, const mode_t mode) {
    LogRegistry::fuse()->debug("[mkdir] Called for parent: {}, name: {}, mode: {}", parent, name, mode);

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;

    const auto user = UserQueries::getUserByLinuxUID(uid);
    if (!user) {
        LogRegistry::fuse()->error("[mkdir] No user found for UID: {}", uid);
        fuse_reply_err(req, EACCES);
        return;
    }

    try {
        if (std::string_view(name).find('/') != std::string::npos) {
            fuse_reply_err(req, EINVAL);
            return;
        }

        const auto& cache = ServiceDepsRegistry::instance().fsCache;

        const auto parentPath = cache->resolvePath(parent);
        if (parentPath.empty()) {
            fuse_reply_err(req, ENOENT);
            return;
        }

        const std::filesystem::path fullPath = parentPath / name;

        const auto engine = ServiceDepsRegistry::instance().storageManager->resolveStorageEngine(fullPath);
        if (!engine) {
            LogRegistry::fuse()->error("[mkdir] No storage engine found for path: {}", fullPath.string());
            fuse_reply_err(req, EIO);
            return;
        }

        const auto vaultPath = engine->paths->absRelToAbsRel(fullPath, PathType::FUSE_ROOT, PathType::VAULT_ROOT);
        if (!user->canManageVaults() && !user->canCreateVaultData(engine->vault->id, vaultPath)) {
            LogRegistry::fuse()->warn("[mkdir] Access denied for user {} on path {}", user->name, fullPath.string());
            fuse_reply_err(req, EACCES);
            return;
        }

        try {
            Filesystem::mkdir(fullPath, mode);
        } catch (const std::filesystem::filesystem_error& fsErr) {
            LogRegistry::fuse()->error("[mkdir] Failed to create directory: {} → {}: {}",
                parentPath.string(), name, fsErr.what());
            fuse_reply_err(req, EIO);
            return;
        }

        // Final directory (last one created) = fullPath
        const auto finalInode = cache->resolveInode(fullPath);
        const auto finalEntry = cache->getEntry(fullPath);

        fuse_entry_param e{};
        e.ino = finalInode;
        e.attr_timeout = 1.0;
        e.entry_timeout = 1.0;
        e.attr = statFromEntry(finalEntry, finalInode);

        fuse_reply_entry(req, &e);

    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[mkdir] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void rename(const fuse_req_t req, const fuse_ino_t parent, const char* name, const fuse_ino_t newparent, const char* newname, const unsigned int flags) {
    LogRegistry::fuse()->debug("[rename] Called for parent: {}, name: {}, newparent: {}, newname: {}, flags: {}",
                               parent, name, newparent, newname, flags);

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    try {
        const auto fromPath = cache->resolvePath(parent) / name;
        const auto toPath = cache->resolvePath(newparent) / newname;

        // Flags handling (RENAME_NOREPLACE = 1, RENAME_EXCHANGE = 2)
        if ((flags & RENAME_NOREPLACE) && cache->entryExists(toPath)) {
            fuse_reply_err(req, EEXIST);
            return;
        }

        const auto entry = cache->getEntry(fromPath);
        if (!entry) {
            fuse_reply_err(req, ENOENT);
            return;
        }

        const auto user = UserQueries::getUserByLinuxUID(uid);
        if (!user) {
            LogRegistry::fuse()->error("[rename] No user found for UID: {}", uid);
            fuse_reply_err(req, EACCES);
            return;
        }

        if (!user->canManageVaults() && entry->vault_id && !user->canRenameVaultData(*entry->vault_id, entry->path)) {
            LogRegistry::fuse()->warn("[rename] Access denied for user {} on path {}", user->name, entry->path.string());
            fuse_reply_err(req, EACCES);
            return;
        }

        try {
            Filesystem::rename(fromPath, toPath);
        } catch (const std::filesystem::filesystem_error& fsErr) {
            LogRegistry::fuse()->error("[rename] Failed to rename: {} → {}: {}",
                fromPath.string(), toPath.string(), fsErr.what());
            fuse_reply_err(req, EIO);
            return;
        }

        fuse_reply_err(req, 0);  // Success
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[rename] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void forget(const fuse_req_t req, const fuse_ino_t ino, const uint64_t nlookup) {
    LogRegistry::fuse()->debug("[forget] Called for inode: {}, nlookup: {}", ino, nlookup);
    if (const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(ino)) {
        LogRegistry::fuse()->debug("[forget] Evicting inode: {} (path: {})", ino, entry->path.string());
        ServiceDepsRegistry::instance().fsCache->evictIno(ino);
        fuse_reply_err(req, 0);
        return;
    }

    LogRegistry::fuse()->error("[forget] No entry found for inode {}", ino);
    fuse_reply_none(req); // no return value
}

void access(const fuse_req_t req, const fuse_ino_t ino, const int mask) {
    LogRegistry::fuse()->debug("[access] Called for inode: {}, mask: {}", ino, mask);

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;

    const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(ino);
    if (!entry) {
        LogRegistry::fuse()->error("[access] No entry found for inode {}", ino);
        fuse_reply_err(req, ENOENT);
        return;
    }

    const auto user = UserQueries::getUserByLinuxUID(uid);
    if (!user) {
        LogRegistry::fuse()->error("[access] No user found for UID: {}", uid);
        fuse_reply_err(req, EACCES);
        return;
    }

    if (!user->canManageVaults() && entry->vault_id) {
        bool allowed = true;
        if ((mask & R_OK) && !user->canDownloadVaultData(*entry->vault_id, entry->path)) allowed = false;
        if ((mask & W_OK) && !user->canCreateVaultData(*entry->vault_id, entry->path)) allowed = false;
        if ((mask & X_OK) && !user->canListVaultData(*entry->vault_id, entry->path)) allowed = false;

        if (!allowed) {
            LogRegistry::fuse()->warn("[access] Access denied for user {} on path {}, mask: {}",
                user->name, entry->path.string(), mask);
            fuse_reply_err(req, EACCES);
            return;
        }
    }

    fuse_reply_err(req, 0); // Access checks are not implemented, always allow
}

void unlink(const fuse_req_t req, const fuse_ino_t parent, const char* name) {
    const fuse_ctx *ctx = fuse_req_ctx(req);
    const uid_t uid = ctx->uid;
    // const gid_t gid = ctx->gid;

    LogRegistry::fuse()->debug("[unlink] Called for parent: {}, name: {}", parent, name);

    if (!name || strlen(name) == 0) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    try {
        const auto& cache = ServiceDepsRegistry::instance().fsCache;

        const auto parentPath = cache->resolvePath(parent);
        const auto fullPath   = parentPath / name;

        if (!cache->entryExists(fullPath)) {
            LogRegistry::fuse()->debug("[unlink] Entry does not exist for path: {}", fullPath.string());
            fuse_reply_err(req, ENOENT);
            return;
        }

        const auto file = cache->getEntry(fullPath);
        if (!file || file->isDirectory()) {
            fuse_reply_err(req, EISDIR);
            return;
        }

        const auto user = UserQueries::getUserByLinuxUID(uid);
        if (!user) {
            LogRegistry::fuse()->error("[unlink] No user found for UID: {}", uid);
            fuse_reply_err(req, EACCES);
            return;
        }

        if (!user->canManageVaults() && file->vault_id && !user->canDeleteVaultData(*file->vault_id, file->path)) {
            LogRegistry::fuse()->warn("[unlink] Access denied for user {} on path {}", user->name, file->path.string());
            fuse_reply_err(req, EACCES);
            return;
        }

        FileQueries::markFileAsTrashed(user->id, *file->vault_id, file->path, true);

        if (::unlink(file->backing_path.c_str()) < 0)
            LogRegistry::fuse()->debug("[unlink] Failed to remove backing file: {}: {}", file->backing_path.string(), strerror(errno));

        fuse_reply_err(req, 0);
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[unlink] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void rmdir(const fuse_req_t req, const fuse_ino_t parent, const char* name) {
    const fuse_ctx *ctx = fuse_req_ctx(req);
    const uid_t uid = ctx->uid;
    // const gid_t gid = ctx->gid;

    LogRegistry::fuse()->debug("[rmdir] Called for parent: {}, name: {}", parent, name);

    if (!name || strlen(name) == 0) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    try {
        const auto& cache = ServiceDepsRegistry::instance().fsCache;

        const auto parentPath = cache->resolvePath(parent);
        const auto fullPath   = parentPath / name;

        if (!cache->entryExists(fullPath)) {
            fuse_reply_err(req, ENOENT);
            return;
        }

        const auto entry = cache->getEntry(fullPath);
        if (!entry || !entry->isDirectory()) {
            fuse_reply_err(req, ENOTDIR);
            return;
        }

        if (!DirectoryQueries::isDirectoryEmpty(entry->id)) {
            fuse_reply_err(req, ENOTEMPTY);
            return;
        }

        const auto user = UserQueries::getUserByLinuxUID(uid);
        if (!user) {
            LogRegistry::fuse()->error("[rmdir] No user found for UID: {}", uid);
            fuse_reply_err(req, EACCES);
            return;
        }

        if (!user->canManageVaults() && entry->vault_id && !user->canDeleteVaultData(*entry->vault_id, entry->path)) {
            LogRegistry::fuse()->warn("[rmdir] Access denied for user {} on path {}", user->name, entry->path.string());
            fuse_reply_err(req, EACCES);
            return;
        }

        DirectoryQueries::deleteEmptyDirectory(entry->id);

        if (::rmdir(entry->backing_path.c_str()) < 0)
            LogRegistry::fuse()->debug("[rmdir] Failed to remove backing directory: {}: {}", entry->backing_path.string(), strerror(errno));

        fuse_reply_err(req, 0);
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[rmdir] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void flush(const fuse_req_t req, const fuse_ino_t ino, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[flush] Called for inode: {}, file handle: {}", ino, fi->fh);

    // This can be a no-op unless something like sync(2) or call fsync(fd) is desired
    // Likely no need finalize here since flush may be called multiple times per FD

    fuse_reply_err(req, 0);
}

void release(const fuse_req_t req, const fuse_ino_t ino, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[release] Called for inode: {}, file handle: {}", ino, fi->fh);

    const auto* fh = reinterpret_cast<FileHandle*>(fi->fh);
    if (!fh) {
        LogRegistry::fuse()->error("[release] Invalid file handle for inode: {}", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    if (::close(fh->fd) < 0)
        LogRegistry::fuse()->error("[release] Failed to close file handle: {}: {}", fh->path.string(), strerror(errno));

    delete fh;  // clean up heap allocation
    fi->fh = 0; // clear the kernel-side handle

    ServiceDepsRegistry::instance().storageManager->closeOpenHandle(ino);

    fuse_reply_err(req, 0);
}

void fsync(const fuse_req_t req, const fuse_ino_t ino, const int datasync, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[fsync] Called for inode: {}, file handle: {}, isdatasync: {}", ino, fi->fh, datasync);

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;

    (void) datasync;

    const auto user = UserQueries::getUserByLinuxUID(uid);
    if (!user) {
        LogRegistry::fuse()->error("[fsync] No user found for UID: {}", uid);
        fuse_reply_err(req, EACCES);
        return;
    }

    const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(ino);
    if (!entry) {
        LogRegistry::fuse()->error("[fsync] No entry found for inode {}", ino);
        fuse_reply_err(req, ENOENT);
        return;
    }

    if (!user->canManageVaults() && entry->vault_id && !user->canCreateVaultData(*entry->vault_id, entry->path)) {
        LogRegistry::fuse()->warn("[fsync] Access denied for user {} on path {}", user->name, entry->path.string());
        fuse_reply_err(req, EACCES);
        return;
    }

    try {
        if (const int fd = fi->fh; ::fsync(fd) < 0) {
            LogRegistry::fuse()->error("[fsync] Failed to sync file handle: {}: {}", fd, strerror(errno));
            fuse_reply_err(req, errno);
            return;
        }

        fuse_reply_err(req, 0);
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[fsync] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void statfs(const fuse_req_t req, const fuse_ino_t ino) {
    LogRegistry::fuse()->debug("[statfs] Called for inode: {}", ino);

    const fuse_ctx* ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;

    try {
        struct statvfs st{};
        const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(ino);
        if (!entry) {
            LogRegistry::fuse()->error("[statfs] No entry found for inode {}", ino);
            fuse_reply_err(req, ENOENT);
            return;
        }

        const auto user = UserQueries::getUserByLinuxUID(uid);
        if (!user) {
            LogRegistry::fuse()->error("[statfs] No user found for UID: {}", uid);
            fuse_reply_err(req, EACCES);
            return;
        }

        if (!user->canManageVaults() && entry->vault_id &&
            (!user->canListVaultData(*entry->vault_id, entry->path) || !user->canDownloadVaultData(*entry->vault_id, entry->path))) {
            LogRegistry::fuse()->warn("[statfs] Access denied for user {} on path {}", user->name, entry->path.string());
            fuse_reply_err(req, EACCES);
            return;
        }

        if (::statvfs(entry->backing_path.c_str(), &st) < 0) {
            LogRegistry::fuse()->error("[statfs] Failed to get filesystem stats for: {}: {}", entry->backing_path.string(), strerror(errno));
            fuse_reply_err(req, errno);
            return;
        }

        fuse_reply_statfs(req, &st);
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[statfs] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

fuse_lowlevel_ops getOperations() {
    fuse_lowlevel_ops ops = {};
    ops.getattr = getattr;
    ops.setattr = setattr;
    ops.readdir = readdir;
    ops.lookup = lookup;
    ops.open = open;
    ops.read = read;
    ops.forget = forget;
    ops.write = write;
    ops.create = create;
    ops.release = release;
    ops.access = access;
    ops.mkdir = mkdir;
    ops.rename = rename;
    ops.unlink = unlink;
    ops.rmdir = rmdir;
    ops.flush = flush;
    ops.fsync = fsync;
    ops.statfs = statfs;
    return ops;
}

struct stat statFromEntry(const std::shared_ptr<Entry>& entry, const fuse_ino_t& ino) {
    struct stat st{};
    st.st_ino = ino;
    st.st_mode = entry->isDirectory() ? S_IFDIR | 0755 : S_IFREG | 0644;
    st.st_size = static_cast<off_t>(entry->size_bytes);
    st.st_mtim.tv_sec = entry->updated_at;
    st.st_atim = st.st_ctim = st.st_mtim;
    st.st_nlink = 1;
    return st;
}

}

#include "storage/FUSEBridge.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Vault.hpp"
#include "types/FSEntry.hpp"
#include "types/Directory.hpp"
#include "types/Path.hpp"
#include "util/fsPath.hpp"
#include "config/ConfigRegistry.hpp"
#include "storage/Filesystem.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "database/Queries/UserQueries.hpp"
#include "logging/LogRegistry.hpp"

#include <cerrno>
#include <cstring>
#include <sys/statvfs.h>
#include <unistd.h>

#define FUSE_DISPATCH(op) \
[](fuse_req_t req, auto... args) { \
auto* self = static_cast<FUSEBridge*>(fuse_req_userdata(req)); \
return self->op(req, args...); \
}

using namespace vh::database;
using namespace vh::fuse;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::config;
using namespace vh::services;
using namespace vh::logging;

FUSEBridge::FUSEBridge(const std::shared_ptr<StorageManager>& storageManager)
    : storageManager_(storageManager) {}

void FUSEBridge::getattr(const fuse_req_t& req, const fuse_ino_t& ino, fuse_file_info* fi) const {
    const auto log = LogRegistry::fuse();
    log->debug("[getattr] Called for inode: {}", ino);

    try {
        const auto& cache = ServiceDepsRegistry::instance().fsCache;
        if (ino == FUSE_ROOT_ID) {
            const auto entry = cache->getEntry("/");
            if (!entry) {
                log->error("[getattr] No entry found for inode {}, resolved path: /", ino);
                fuse_reply_err(req, ENOENT);
                return;
            }

            struct stat st = statFromEntry(entry, ino);
            st.st_uid = getuid();  // explicitly set ownership
            st.st_gid = getgid();

            fuse_reply_attr(req, &st,  0.1);
            return;
        }

        const auto path = cache->resolvePath(ino);
        if (path.empty()) {
            log->error("[getattr] No path resolved for inode {}, returning ENOENT", ino);
            fuse_reply_err(req, ENOENT);
            return;
        }

        const auto entry = cache->getEntry(path);

        if (!entry) {
            log->error("[getattr] No entry found for inode {}, resolved path: {}", ino, path.string());
            fuse_reply_err(req, ENOENT);
            return;
        }

        struct stat st = {};
        st.st_ino = ino;
        st.st_mode = entry->isDirectory() ? S_IFDIR | 0755 : S_IFREG | 0644;
        st.st_nlink = 1;
        st.st_size = static_cast<off_t>(entry->size_bytes);
        st.st_mtim.tv_sec = entry->updated_at;
        st.st_atim = st.st_ctim = st.st_mtim;

        fuse_reply_attr(req, &st, 0.1); // match attr_timeout from lookup()
    } catch (const std::exception& ex) {
        fuse_reply_err(req, ENOENT);
    }
}

void FUSEBridge::setattr(fuse_req_t req, fuse_ino_t ino,
                         struct stat* attr, int to_set, fuse_file_info* fi) const {
    const fuse_ctx* ctx = fuse_req_ctx(req);

    std::cout << "[setattr] Called for inode: " << ino
              << " mask=" << to_set << std::endl;

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
        const auto& cache = ServiceDepsRegistry::instance().fsCache;
        const auto path   = cache->resolvePath(ino);
        if (path.empty()) {
            fuse_reply_err(req, ENOENT);
            return;
        }

        const auto backingPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(path);

        timespec times[2];
        if (to_set & FUSE_SET_ATTR_ATIME) times[0] = attr->st_atim;
        else times[0].tv_nsec = UTIME_OMIT;
        if (to_set & FUSE_SET_ATTR_MTIME) times[1] = attr->st_mtim;
        else times[1].tv_nsec = UTIME_OMIT;

        if (::utimensat(AT_FDCWD, backingPath.c_str(), times, 0) < 0) {
            fuse_reply_err(req, errno);
            return;
        }

        // Re-stat file so kernel gets fresh info
        struct stat st;
        if (::stat(backingPath.c_str(), &st) < 0) {
            fuse_reply_err(req, errno);
            return;
        }

        fuse_reply_attr(req, &st, 1.0);
    } catch (...) {
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::readdir(const fuse_req_t& req, fuse_ino_t ino, size_t size, off_t off, fuse_file_info* fi) const {
    LogRegistry::fuse()->debug("[readdir] Called for inode: {}, size: {}, offset: {}", ino, size, off);
    (void)fi;

    const auto path = ServiceDepsRegistry::instance().fsCache->resolvePath(ino);
    auto entries = FSEntryQueries::listDir(path, false);

    std::vector<char> buf(size);
    size_t buf_used = 0;

    auto add_entry = [&](const std::string& name, const struct stat& st, off_t next_off) {
        size_t entry_size = fuse_add_direntry(req, nullptr, 0, name.c_str(), &st, next_off);
        if (buf_used + entry_size > size) return false;

        fuse_add_direntry(req, buf.data() + buf_used, entry_size, name.c_str(), &st, next_off);
        buf_used += entry_size;
        return true;
    };

    off_t current_off = 0;

    if (off <= current_off++) {
        struct stat dot = { .st_mode = S_IFDIR };
        if (!add_entry(".", dot, current_off)) goto reply;
    }

    if (off <= current_off++) {
        struct stat dotdot = { .st_mode = S_IFDIR };
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

void FUSEBridge::lookup(const fuse_req_t& req, const fuse_ino_t& parent, const char* name) const {
    LogRegistry::fuse()->debug("[lookup] Called for parent: {}, name: {}", parent, name);
    if (!name || strlen(name) == 0) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    const auto parentPath = cache->resolvePath(parent);
    const auto path = parentPath / name;
    const fuse_ino_t ino = cache->getOrAssignInode(path);

    const auto entry = cache->getEntry(path);
    if (!entry) {
        LogRegistry::fuse()->error("[lookup] Entry not found for path: {}", path.string());
        fuse_reply_err(req, ENOENT);
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

void FUSEBridge::create(const fuse_req_t& req, fuse_ino_t parent,
                        const char* name, mode_t mode, struct fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[create] Called for parent: {}, name: {}, mode: {}", parent, name, mode);

    if (!name || strlen(name) == 0) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    try {
        fs::path parentPath = cache->resolvePath(parent);
        fs::path fullPath   = parentPath / name;

        if (cache->entryExists(fullPath)) {
            fuse_reply_err(req, EEXIST);
            return;
        }

        auto newEntry = Filesystem::createFile(fullPath, getuid(), getgid(), mode);
        auto st       = statFromEntry(newEntry, *newEntry->inode);

        // open backing file immediately
        auto backingPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(fullPath);
        int fd = ::open(backingPath.c_str(), O_CREAT | O_RDWR, mode);
        if (fd < 0) {
            fuse_reply_err(req, errno);
            return;
        }

        auto* fh = new FileHandle{backingPath.string(), fd};
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

void FUSEBridge::open(const fuse_req_t& req, const fuse_ino_t& ino, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[open] Called for inode: {}, flags: {}", ino, fi->flags);

    {
        std::scoped_lock lock(openHandleMutex_);
        openHandleCounts_[ino]++;
    }

    try {
        const fs::path path = ServiceDepsRegistry::instance().fsCache->resolvePath(ino);
        const auto backingPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(path);

        int fd = ::open(backingPath.c_str(), fi->flags & O_ACCMODE);
        if (fd < 0) {
            fuse_reply_err(req, errno);
            return;
        }

        auto* fh = new FileHandle{backingPath.string(), fd};
        fi->fh = reinterpret_cast<uint64_t>(fh);

        fi->direct_io = 1;
        fi->keep_cache = 0;

        fuse_reply_open(req, fi);
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[open] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::write(fuse_req_t req, fuse_ino_t ino, const char* buf,
                       size_t size, off_t off, fuse_file_info* fi) {
    const auto* fh = reinterpret_cast<FileHandle*>(fi->fh);

    const ssize_t res = ::pwrite(fh->fd, buf, size, off);
    if (res < 0) fuse_reply_err(req, errno);
    else fuse_reply_write(req, res);
}

void FUSEBridge::read(const fuse_req_t& req, fuse_ino_t ino, size_t size,
                      off_t off, fuse_file_info* fi) const {
    const auto* fh = reinterpret_cast<FileHandle*>(fi->fh);

    std::vector<char> buffer(size);
    const ssize_t res = ::pread(fh->fd, buffer.data(), size, off);

    if (res < 0) fuse_reply_err(req, errno);
    else fuse_reply_buf(req, buffer.data(), res);
}

void FUSEBridge::mkdir(const fuse_req_t& req, const fuse_ino_t& parent, const char* name, mode_t mode) const {
    LogRegistry::fuse()->debug("[mkdir] Called for parent: {}, name: {}, mode: {}", parent, name, mode);

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

        try {
            Filesystem::mkdir(fullPath, mode);
        } catch (const std::filesystem::filesystem_error& fsErr) {
            LogRegistry::fuse()->error("[mkdir] Failed to create directory: {} → {}: {}", parentPath.string(), name, fsErr.what());
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

void FUSEBridge::rename(const fuse_req_t& req,
                        fuse_ino_t parent,
                        const char* name,
                        fuse_ino_t newparent,
                        const char* newname,
                        unsigned int flags) const {
    LogRegistry::fuse()->debug("[rename] Called for parent: {}, name: {}, newparent: {}, newname: {}, flags: {}",
                               parent, name, newparent, newname, flags);

    const auto& cache = ServiceDepsRegistry::instance().fsCache;

    try {
        const auto fromPath = cache->resolvePath(parent) / name;
        const auto toPath = cache->resolvePath(newparent) / newname;

        // Flags handling (RENAME_NOREPLACE = 1, RENAME_EXCHANGE = 2)
        if ((flags & RENAME_NOREPLACE) && cache->entryExists(toPath)) {
            fuse_reply_err(req, EEXIST);
            return;
        }

        // Confirm source exists
        if (!cache->entryExists(fromPath)) {
            fuse_reply_err(req, ENOENT);
            return;
        }

        try {
            Filesystem::rename(fromPath, toPath);
        } catch (const std::filesystem::filesystem_error& fsErr) {
            LogRegistry::fuse()->error("[rename] Failed to rename: {} → {}: {}", fromPath.string(), toPath.string(), fsErr.what());
            fuse_reply_err(req, EIO);
            return;
        }

        fuse_reply_err(req, 0);  // Success
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[rename] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::forget(const fuse_req_t& req, const fuse_ino_t& ino, uint64_t nlookup) const {
    LogRegistry::fuse()->debug("[forget] Called for inode: {}, nlookup: {}", ino, nlookup);
    ServiceDepsRegistry::instance().fsCache->decrementInodeRef(ino, nlookup);
    fuse_reply_none(req); // no return value
}

void FUSEBridge::access(const fuse_req_t& req, const fuse_ino_t& ino, int mask) const {
    LogRegistry::fuse()->debug("[access] Called for inode: {}, mask: {}", ino, mask);
    fuse_reply_err(req, 0); // Access checks are not implemented, always allow
}

void FUSEBridge::unlink(const fuse_req_t& req, fuse_ino_t parent, const char* name) const {
    const fuse_ctx *ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;
    gid_t gid = ctx->gid;

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
            fuse_reply_err(req, ENOENT);
            return;
        }

        auto backingPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(fullPath);
        if (::unlink(backingPath.c_str()) < 0) {
            LogRegistry::fuse()->error("[unlink] Failed to remove backing file: {}: {}", backingPath.string(), strerror(errno));
            fuse_reply_err(req, errno);
            return;
        }

        Filesystem::remove(fullPath, UserQueries::getUserIdByLinuxUID(uid));

        fuse_reply_err(req, 0);
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[unlink] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::rmdir(const fuse_req_t& req, fuse_ino_t parent, const char* name) const {
    const fuse_ctx *ctx = fuse_req_ctx(req);
    uid_t uid = ctx->uid;
    gid_t gid = ctx->gid;

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

        auto backingPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(fullPath);
        if (::rmdir(backingPath.c_str()) < 0) {
            LogRegistry::fuse()->error("[rmdir] Failed to remove backing directory: {}: {}", backingPath.string(), strerror(errno));
            fuse_reply_err(req, errno);
            return;
        }

        Filesystem::remove(fullPath, UserQueries::getUserIdByLinuxUID(uid));

        fuse_reply_err(req, 0);
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[rmdir] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::flush(const fuse_req_t& req, fuse_ino_t ino, fuse_file_info* fi) const {
    LogRegistry::fuse()->debug("[flush] Called for inode: {}, file handle: {}", ino, fi->fh);

    // This can be a no-op unless you want to sync(2) or call fsync(fd)
    // You usually don’t finalize here since flush may be called multiple times per FD

    fuse_reply_err(req, 0);
}

void FUSEBridge::release(const fuse_req_t& req, fuse_ino_t ino, fuse_file_info* fi) {
    LogRegistry::fuse()->debug("[release] Called for inode: {}, file handle: {}", ino, fi->fh);

    auto* fh = reinterpret_cast<FileHandle*>(fi->fh);
    if (!fh) {
        LogRegistry::fuse()->error("[release] Invalid file handle for inode: {}", ino);
        fuse_reply_err(req, EBADF);
        return;
    }

    if (::close(fh->fd) < 0)
        LogRegistry::fuse()->error("[release] Failed to close file handle: {}: {}", fh->path.string(), strerror(errno));

    delete fh;  // clean up heap allocation
    fi->fh = 0; // clear the kernel-side handle

    {
        std::scoped_lock lock(openHandleMutex_);
        if (--openHandleCounts_[ino] == 0)
            openHandleCounts_.erase(ino);
    }

    fuse_reply_err(req, 0);
}

void FUSEBridge::fsync(const fuse_req_t& req, fuse_ino_t ino, int isdatasync, fuse_file_info* fi) const {
    LogRegistry::fuse()->debug("[fsync] Called for inode: {}, file handle: {}, isdatasync: {}", ino, fi->fh, isdatasync);
    (void) isdatasync; // if we want to treat differently

    try {
        int fd = fi->fh;
        if (::fsync(fd) < 0) {
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

void FUSEBridge::statfs(const fuse_req_t& req, fuse_ino_t ino) const {
    LogRegistry::fuse()->debug("[statfs] Called for inode: {}", ino);
    try {
        struct statvfs st{};
        auto backingPath = ConfigRegistry::get().fuse.backing_path;

        if (::statvfs(backingPath.c_str(), &st) < 0) {
            LogRegistry::fuse()->error("[statfs] Failed to get filesystem stats for: {}: {}", backingPath.string(), strerror(errno));
            fuse_reply_err(req, errno);
            return;
        }

        fuse_reply_statfs(req, &st);
    } catch (const std::exception& ex) {
        LogRegistry::fuse()->error("[statfs] Exception: {}", ex.what());
        fuse_reply_err(req, EIO);
    }
}

fuse_lowlevel_ops FUSEBridge::getOperations() const {
    fuse_lowlevel_ops ops = {};
    ops.getattr = FUSE_DISPATCH(getattr);
    ops.readdir = FUSE_DISPATCH(readdir);
    ops.lookup = FUSE_DISPATCH(lookup);
    ops.mkdir = FUSE_DISPATCH(mkdir);
    ops.create = FUSE_DISPATCH(create);
    ops.rename = FUSE_DISPATCH(rename);
    ops.open = FUSE_DISPATCH(open);
    ops.read = FUSE_DISPATCH(read);
    ops.write = FUSE_DISPATCH(write);
    ops.forget = FUSE_DISPATCH(forget);
    ops.access = FUSE_DISPATCH(access);
    ops.flush = FUSE_DISPATCH(flush);
    ops.release = FUSE_DISPATCH(release);
    ops.setattr = FUSE_DISPATCH(setattr);
    ops.unlink = FUSE_DISPATCH(unlink);
    ops.rmdir = FUSE_DISPATCH(rmdir);
    ops.fsync = FUSE_DISPATCH(fsync);
    ops.statfs = FUSE_DISPATCH(statfs);
    return ops;
}

struct stat FUSEBridge::statFromEntry(const std::shared_ptr<FSEntry>& entry, const fuse_ino_t& ino) const {
    struct stat st = {};
    st.st_ino = ino;
    st.st_mode = entry->isDirectory() ? S_IFDIR | 0755 : S_IFREG | 0644;
    st.st_size = static_cast<off_t>(entry->size_bytes);
    st.st_mtim.tv_sec = entry->updated_at;
    st.st_atim = st.st_ctim = st.st_mtim;
    st.st_nlink = 1;
    return st;
}

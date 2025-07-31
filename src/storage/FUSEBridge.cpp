#include "storage/FUSEBridge.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Vault.hpp"
#include "types/File.hpp"
#include "types/FSEntry.hpp"
#include "types/Directory.hpp"
#include "types/Path.hpp"
#include "util/fsPath.hpp"
#include "util/files.hpp"
#include "config/ConfigRegistry.hpp"
#include "storage/VaultEncryptionManager.hpp"

#include <boost/beast/core/file.hpp>
#include <cerrno>
#include <cstring>
#include <sys/statvfs.h>
#include <unistd.h>
#include <iostream>

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

FUSEBridge::FUSEBridge(const std::shared_ptr<StorageManager>& storageManager)
    : storageManager_(storageManager) {}

void FUSEBridge::getattr(const fuse_req_t& req, const fuse_ino_t& ino, fuse_file_info* fi) const {
    std::cout << "[getattr] Called for inode: " << ino << std::endl;
    try {
        if (ino == FUSE_ROOT_ID) {
            const auto entry = storageManager_->getEntry("/");
            if (!entry) {
                std::cerr << "[getattr] No entry found for inode " << ino
                          << ", resolved path: " << "/" << std::endl;
                fuse_reply_err(req, ENOENT);
                return;
            }

            struct stat st = statFromEntry(entry, ino);
            st.st_uid = getuid();  // explicitly set ownership
            st.st_gid = getgid();

            fuse_reply_attr(req, &st,  0.1);
            return;
        }

        const auto path = storageManager_->resolvePathFromInode(ino);
        if (path.empty()) {
            std::cerr << "[getattr] No path found for inode " << ino << std::endl;
            fuse_reply_err(req, ENOENT);
            return;
        }

        const auto entry = storageManager_->getEntry(path);

        if (!entry) {
            std::cerr << "[getattr] No entry found for inode " << ino
                      << ", resolved path: " << path << std::endl;
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

void FUSEBridge::readdir(const fuse_req_t& req, fuse_ino_t ino, size_t size, off_t off, fuse_file_info* fi) const {
    std::cout << "[FUSE] readdir called for inode: " << ino << ", size: " << size << ", offset: " << off << std::endl;
    (void)fi;

    const auto path = storageManager_->resolvePathFromInode(ino);
    auto entries = storageManager_->listDir(path, false);

    for (const auto& entry : entries) {
        std::cout << "[FUSE] Entry: " << entry->name << ", inode: " << *entry->inode << std::endl;
    }

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
    std::cout << "[FUSE] lookup called for parent: " << parent << ", name: " << name << std::endl;
    if (!name || strlen(name) == 0) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    const auto parentPath = storageManager_->resolvePathFromInode(parent);
    const auto path = parentPath / name;
    const fuse_ino_t ino = storageManager_->getOrAssignInode(path);

    const auto entry = storageManager_->getEntry(path);
    if (!entry) {
        std::cerr << "[FUSE] lookup failed: entry not found at path: " << path << "\n";
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
    std::cout << "[FUSE] create called. parent=" << parent
              << " name=" << (name ? name : "(null)")
              << " mode=" << std::oct << mode << std::endl;

    if (!name || strlen(name) == 0) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    try {
        fs::path parentPath = storageManager_->resolvePathFromInode(parent);
        fs::path fullPath   = parentPath / name;

        if (storageManager_->entryExists(fullPath)) {
            fuse_reply_err(req, EEXIST);
            return;
        }

        auto newEntry = storageManager_->createFile(fullPath, mode, getuid(), getgid());
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
        std::cerr << "[FUSE] create failed: " << ex.what() << std::endl;
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::open(const fuse_req_t& req, const fuse_ino_t& ino, fuse_file_info* fi) {
    std::cout << "[FUSE] open called for inode: " << ino << std::endl;
    try {
        const fs::path path = storageManager_->resolvePathFromInode(ino);
        const auto backingPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(path);

        int fd = ::open(backingPath.c_str(), O_RDWR);
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
        std::cerr << "[FUSE] open failed: " << ex.what() << std::endl;
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::write(fuse_req_t req, fuse_ino_t ino, const char* buf,
                       size_t size, off_t off, fuse_file_info* fi) {
    const auto* fh = reinterpret_cast<FileHandle*>(fi->fh);
    std::cout << "[FUSE] write ino=" << ino
              << " off=" << off << " size=" << size
              << " path=" << fh->path << std::endl;

    const ssize_t res = ::pwrite(fh->fd, buf, size, off);
    if (res < 0) fuse_reply_err(req, errno);
    else fuse_reply_write(req, res);
}

void FUSEBridge::read(const fuse_req_t& req, fuse_ino_t ino, size_t size,
                      off_t off, fuse_file_info* fi) const {
    const auto* fh = reinterpret_cast<FileHandle*>(fi->fh);
    std::cout << "[FUSE] read ino=" << ino
              << " off=" << off << " size=" << size
              << " path=" << fh->path << std::endl;

    std::vector<char> buffer(size);
    const ssize_t res = ::pread(fh->fd, buffer.data(), size, off);

    if (res < 0) fuse_reply_err(req, errno);
    else fuse_reply_buf(req, buffer.data(), res);
}

void FUSEBridge::mkdir(const fuse_req_t& req, const fuse_ino_t& parent, const char* name, mode_t mode) const {
    std::cout << "[FUSE] mkdir called for parent inode: " << parent << std::endl;
    try {
        if (std::string_view(name).find('/') != std::string::npos) {
            fuse_reply_err(req, EINVAL);
            return;
        }

        const auto parentPath = storageManager_->resolvePathFromInode(parent);
        if (parentPath.empty()) {
            fuse_reply_err(req, ENOENT);
            return;
        }

        const std::filesystem::path fullPath = parentPath / name;

        // ---------------------------
        // Recursive directory creation
        // ---------------------------
        std::vector<std::filesystem::path> toCreate;
        std::filesystem::path cur = fullPath;

        // Traverse upwards until we find an existing entry
        while (!cur.empty() && !storageManager_->entryExists(cur)) {
            toCreate.push_back(cur);
            cur = cur.parent_path();
        }

        // Now create from top-down (reverse order)
        std::ranges::reverse(toCreate.begin(), toCreate.end());

        for (const auto& path : toCreate) {
            auto dir = std::make_shared<Directory>();

            if (const auto engine = storageManager_->resolveStorageEngine(path)) {
                dir->vault_id = engine->vault->id;
                dir->path = engine->paths->relPath(path, PathType::VAULT_ROOT);
            }

            if (auto parentEntry = storageManager_->getEntry(resolveParent(path)))
                dir->parent_id = parentEntry->id;

            const auto fullDiskPath = ConfigRegistry::get().fuse.backing_path / stripLeadingSlash(path);

            dir->abs_path = makeAbsolute(path);
            dir->name = path.filename();
            dir->created_at = dir->updated_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = storageManager_->assignInode(path);
            dir->is_hidden = false;
            dir->is_system = false;

            storageManager_->cacheEntry(dir);
            DirectoryQueries::upsertDirectory(dir);

            try {
                std::filesystem::create_directory(fullDiskPath); // Make just this one level
            } catch (const std::filesystem::filesystem_error& fsErr) {
                std::cerr << "[mkdir] Filesystem error: " << fsErr.what() << " for path: " << fullDiskPath << std::endl;
                fuse_reply_err(req, EIO);
                return;
            }
        }

        // Final directory (last one created) = fullPath
        const auto finalInode = storageManager_->resolveInode(fullPath);
        const auto finalEntry = storageManager_->getEntry(fullPath);

        fuse_entry_param e{};
        e.ino = finalInode;
        e.attr_timeout = 1.0;
        e.entry_timeout = 1.0;
        e.attr = statFromEntry(finalEntry, finalInode);

        fuse_reply_entry(req, &e);

    } catch (const std::exception& ex) {
        std::cerr << "[mkdir] Exception: " << ex.what() << "\n";
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::rename(const fuse_req_t& req,
                        fuse_ino_t parent,
                        const char* name,
                        fuse_ino_t newparent,
                        const char* newname,
                        unsigned int flags) const {
    std::cout << "[FUSE] rename called: " << name << " → " << newname << std::endl;

    try {
        const auto fromPath = storageManager_->resolvePathFromInode(parent) / name;
        const auto toPath = storageManager_->resolvePathFromInode(newparent) / newname;

        // Flags handling (RENAME_NOREPLACE = 1, RENAME_EXCHANGE = 2)
        if ((flags & RENAME_NOREPLACE) && storageManager_->entryExists(toPath)) {
            fuse_reply_err(req, EEXIST);
            return;
        }

        // Confirm source exists
        if (!storageManager_->entryExists(fromPath)) {
            fuse_reply_err(req, ENOENT);
            return;
        }

        try {
            storageManager_->renamePath(fromPath, toPath);
        } catch (const std::filesystem::filesystem_error& fsErr) {
            std::cerr << "[FUSE] rename failed: " << fsErr.what() << " for path: " << fromPath << " → " << toPath << std::endl;
            fuse_reply_err(req, EIO);
            return;
        }

        fuse_reply_err(req, 0);  // Success
    } catch (const std::exception& ex) {
        std::cerr << "[FUSE] rename failed: " << ex.what() << std::endl;
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::forget(const fuse_req_t& req, const fuse_ino_t& ino, uint64_t nlookup) const {
    storageManager_->decrementInodeRef(ino, nlookup);
    fuse_reply_none(req); // no return value
}

void FUSEBridge::access(const fuse_req_t& req, const fuse_ino_t& ino, int mask) const {
    fuse_reply_err(req, 0); // Access checks are not implemented, always allow
}

void FUSEBridge::flush(const fuse_req_t& req, fuse_ino_t ino, fuse_file_info* fi) const {
    std::cout << "[FUSE] flush called for inode: " << ino << std::endl;

    // This can be a no-op unless you want to sync(2) or call fsync(fd)
    // You usually don’t finalize here since flush may be called multiple times per FD

    fuse_reply_err(req, 0);
}

void FUSEBridge::release(const fuse_req_t& req, fuse_ino_t ino, fuse_file_info* fi) {
    std::cout << "[FUSE] release called for inode: " << ino << std::endl;

    auto* fh = reinterpret_cast<FileHandle*>(fi->fh);
    if (!fh) {
        std::cerr << "[release] Invalid file handle (null)" << std::endl;
        fuse_reply_err(req, EBADF);
        return;
    }

    if (const auto rename = storageManager_->getPendingRename(ino))
        storageManager_->updatePaths(rename->oldPath, rename->newPath);

    if (::close(fh->fd) < 0)
        std::cerr << "[release] Failed to close FD: " << strerror(errno) << std::endl;

    delete fh;  // clean up heap allocation
    fi->fh = 0; // clear the kernel-side handle

    {
        std::scoped_lock lock(openHandleMutex_);
        if (--openHandleCounts_[ino] == 0)
            openHandleCounts_.erase(ino);
    }

    fuse_reply_err(req, 0);
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

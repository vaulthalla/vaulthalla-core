#include "control/FUSEBridge.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Vault.hpp"
#include "types/File.hpp"
#include "types/FSEntry.hpp"
#include "types/Directory.hpp"
#include "util/fsPath.hpp"
#include "util/files.hpp"
#include "config/ConfigRegistry.hpp"
#include "engine/VaultEncryptionManager.hpp"

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

            fuse_reply_attr(req, &st, 60.0);
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

        fuse_reply_attr(req, &st, 60.0); // match attr_timeout from lookup()
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
    e.attr_timeout = 60.0;
    e.entry_timeout = 60.0;
    e.attr = st;

    fuse_reply_entry(req, &e);
}

void FUSEBridge::create(const fuse_req_t& req, fuse_ino_t parent, const char* name, mode_t mode, struct fuse_file_info* fi) {
    std::cout << "[FUSE] create called. parent: " << parent << ", name: " << name << ", mode: " << std::oct << mode << std::endl;

    if (!name || strlen(name) == 0) {
        fuse_reply_err(req, EINVAL);
        return;
    }

    try {
        std::cout << "[FUSE] Resolving parent path" << std::endl;
        const fs::path parentPath = storageManager_->resolvePathFromInode(parent);
        const fs::path fullPath = parentPath / name;

        std::cout << "[FUSE] Full path for new file: " << fullPath << std::endl;

        // Check if file already exists
        if (storageManager_->entryExists(fullPath)) {
            std::cout << "[FUSE] File already exists" << std::endl;
            fuse_reply_err(req, EEXIST);
            return;
        }

        std::cout << "[FUSE] Creating file at path: " << fullPath << std::endl;

        // Create the entry in the storage (this may involve inserting into DB or filesystem layer)
        const auto newEntry = storageManager_->createFile(fullPath, mode, getuid(), getgid());
        const auto st = statFromEntry(newEntry, *newEntry->inode);

        // Fill response
        fuse_entry_param e{};
        e.ino = *newEntry->inode;
        e.attr = st;
        e.attr_timeout = 60.0;
        e.entry_timeout = 60.0;

        fi->fh = static_cast<uint64_t>(*newEntry->inode); // Optional: set file handle to inode
        fi->direct_io = 0;  // You can change this if you want to bypass kernel caching
        fi->keep_cache = 0;

        fuse_reply_create(req, &e, fi);
    } catch (const std::exception& ex) {
        std::cerr << "[FUSE] create failed: " << ex.what() << "\n";
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::open(const fuse_req_t& req, const fuse_ino_t& ino, fuse_file_info* fi) {
    std::cout << "[FUSE] open called for inode: " << ino << std::endl;

    {
        std::scoped_lock lock(openHandleMutex_);
        openHandleCounts_[ino]++;
    }

    try {
        fs::path path = storageManager_->resolvePathFromInode(ino);
        const auto diskPath = ConfigRegistry::get().fuse.root_mount_path / stripLeadingSlash(path);

        int fd = ::open(diskPath.c_str(), O_RDWR);
        if (fd < 0) {
            std::cerr << "Failed to open file: " << path << " (" << strerror(errno) << ")" << std::endl;
            fuse_reply_err(req, errno);
            return;
        }

        std::cout << "[FUSE] open() got fd: " << fd << std::endl;

        fi->fh = static_cast<uint64_t>(fd);  // REQUIRED: set the file handle
        fuse_reply_open(req, fi);            // Only call this if fi is valid

    } catch (...) {
        std::cerr << "Unhandled exception in open()" << std::endl;
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::read(const fuse_req_t& req, const fuse_ino_t& ino, const size_t size, const off_t off, fuse_file_info* fi) const {
    std::cout << "[FUSE] read called for inode: " << ino << ", size: " << size << ", offset: " << off << std::endl;
    try {
        fs::path path = storageManager_->resolvePathFromInode(ino);
        int fd = ::open(path.c_str(), O_RDONLY);
        if (fd == -1) {
            fuse_reply_err(req, errno);
            return;
        }

        std::vector<char> buffer(size);
        ssize_t bytesRead = ::pread(fd, buffer.data(), size, off);
        ::close(fd);

        if (bytesRead == -1) {
            fuse_reply_err(req, errno);
        } else {
            fuse_reply_buf(req, buffer.data(), bytesRead);
        }
    } catch (...) {
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::write(fuse_req_t req, fuse_ino_t ino, const char* buf, size_t size,
                       off_t off, struct fuse_file_info* fi) {
    std::cout << "[FUSE] write called for inode: " << ino << " offset: " << off << " size: " << size << std::endl;

    ssize_t res = pwrite(fi->fh, buf, size, off);
    if (res == -1) {
        fuse_reply_err(req, errno);
    } else {
        fuse_reply_write(req, res);
    }
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
                dir->path = engine->resolveAbsolutePathToVaultPath(path);
            }

            if (auto parentEntry = storageManager_->getEntry(resolveParent(path)))
                dir->parent_id = parentEntry->id;

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
                std::filesystem::create_directory(path); // Make just this one level
            } catch (const std::filesystem::filesystem_error& fsErr) {
                std::cerr << "[mkdir] Filesystem error: " << fsErr.what() << " for path: " << path << std::endl;
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

    int fd = static_cast<int>(fi->fh);
    if (fd < 0) {
        std::cerr << "[release] Invalid file handle: " << fd << std::endl;
        fuse_reply_err(req, EBADF);
        return;
    }

    if (const auto rename = storageManager_->getPendingRename(ino)) {
        std::optional<std::string> iv_b64;
        const auto oldAbsPath = ConfigRegistry::get().fuse.root_mount_path / stripLeadingSlash(rename->oldPath);
        const auto newAbsPath = ConfigRegistry::get().fuse.root_mount_path / stripLeadingSlash(rename->newPath);
        std::cout << "[StorageManager] Processing pending rename: " << oldAbsPath << " → " << newAbsPath << std::endl;

        if (const auto engine = storageManager_->resolveStorageEngine(rename->oldPath)) {
            std::cout << "[StorageManager] Renaming on disk with encryption: " << oldAbsPath << " → " << newAbsPath << std::endl;

            struct stat st{};
            if (fstat(fd, &st) == -1) {
                std::cerr << "[release] fstat failed: " << strerror(errno) << std::endl;
                ::close(fd);
                fuse_reply_err(req, errno);
                return;
            }

            std::vector<uint8_t> buffer(st.st_size);
            if (pread(fd, buffer.data(), buffer.size(), 0) != static_cast<ssize_t>(buffer.size())) {
                std::cerr << "[release] Failed to read file via fd" << std::endl;
                ::close(fd);
                fuse_reply_err(req, EIO);
                return;
            }

            std::string iv;
            const auto ciphertext = engine->encryptionManager->encrypt(buffer, iv);
            iv_b64 = iv;

            util::writeFile(newAbsPath, ciphertext);
            std::filesystem::remove(oldAbsPath);
        } else {
            std::cout << "[StorageManager] Renaming on disk (no encryption): " << oldAbsPath << " → " << newAbsPath << std::endl;
            std::error_code ec;
            std::filesystem::rename(oldAbsPath, newAbsPath, ec);
            if (ec) {
                std::cerr << "[StorageManager] Failed to rename on disk: " << ec.message() << std::endl;
                ::close(fd);
                fuse_reply_err(req, ec.value());
                return;
            }
        }

        storageManager_->updatePaths(rename->oldPath, rename->newPath, iv_b64);
    }

    // Always close fd after finished with it
    if (::close(fd) < 0) {
        std::cerr << "[release] Failed to close FD: " << strerror(errno) << std::endl;
    }

    {
        std::scoped_lock lock(openHandleMutex_);
        if (--openHandleCounts_[ino] == 0) {
            openHandleCounts_.erase(ino);
        }
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

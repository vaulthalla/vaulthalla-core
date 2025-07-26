#include "control/FUSEBridge.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/Vault.hpp"
#include "types/File.hpp"
#include "types/FSEntry.hpp"
#include "types/Directory.hpp"
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

FUSEBridge::FUSEBridge(const std::shared_ptr<StorageManager>& storageManager)
    : storageManager_(storageManager) {}

void FUSEBridge::getattr(const fuse_req_t& req, const fuse_ino_t& ino, fuse_file_info* fi) const {
    try {
        if (ino == FUSE_ROOT_ID) {
            const auto entry = storageManager_->getEntry("/");
            if (!entry) {
                fuse_reply_err(req, ENOENT);
                return;
            }

            struct stat st = statFromEntry(entry, ino);
            st.st_uid = getuid();  // explicitly set ownership
            st.st_gid = getgid();

            fuse_reply_attr(req, &st, 60.0);
            return;
        }

        fs::path path = storageManager_->resolvePathFromInode(ino);
        const auto entry = storageManager_->getEntry(path);

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
    (void)fi;

    const auto path = storageManager_->resolvePathFromInode(ino); // implement this to map inode → path
    auto entries = storageManager_->listDir(path);

    std::vector<char> buf(size);
    size_t buf_used = 0;

    auto add_entry = [&](const std::string& name, const struct stat& st) {
        size_t entry_size = fuse_add_direntry(req, nullptr, 0, name.c_str(), &st, 0);
        if (buf_used + entry_size > size) return false;

        fuse_add_direntry(req, buf.data() + buf_used, entry_size, name.c_str(), &st, static_cast<off_t>(buf_used));
        buf_used += entry_size;
        return true;
    };

    // Always include "." and ".."
    struct stat dot = { .st_mode = S_IFDIR };
    struct stat dotdot = { .st_mode = S_IFDIR };
    if (!add_entry(".", dot) || !add_entry("..", dotdot)) {
        fuse_reply_buf(req, buf.data(), buf_used);
        return;
    }

    for (const auto& entry : entries) {
        const auto st = statFromEntry(entry, ino);
        if (!add_entry(entry->name, st)) break;
    }

    fuse_reply_buf(req, buf.data(), buf_used);
}

void FUSEBridge::lookup(const fuse_req_t& req, const fuse_ino_t& parent, const char* name) const {
    const auto parentPath = storageManager_->resolvePathFromInode(parent);
    const auto path = parentPath / name;
    const fuse_ino_t ino = storageManager_->getOrAssignInode(path);

    if (ino == FUSE_ROOT_ID) {
        const auto entry = storageManager_->getEntry("/");
        if (!entry) {
            fuse_reply_err(req, ENOENT);
            return;
        }

        struct stat st = statFromEntry(entry, ino);
        st.st_uid = getuid();  // explicitly set ownership
        st.st_gid = getgid();

        fuse_reply_attr(req, &st, 60.0);
        return;
    }

    const auto entry = storageManager_->getEntry(path);
    if (!entry) {
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

void FUSEBridge::open(const fuse_req_t& req, const fuse_ino_t& ino, fuse_file_info* fi) const {
    try {
        fs::path path = storageManager_->resolvePathFromInode(ino);
        if (!storageManager_->entryExists(path)) fuse_reply_err(req, ENOENT);
        fuse_reply_open(req, fi);
    } catch (...) {
        fuse_reply_err(req, EIO);
    }
}

void FUSEBridge::read(const fuse_req_t& req, const fuse_ino_t& ino, const size_t size, const off_t off, fuse_file_info* fi) const {
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

void FUSEBridge::mkdir(const fuse_req_t& req, const fuse_ino_t& parent, const char* name, mode_t mode) const {
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

            if (auto parentEntry = storageManager_->getEntry(path.parent_path()))
                dir->parent_id = parentEntry->id;

            dir->abs_path = path;
            dir->name = path.filename();
            dir->created_at = dir->updated_at = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            dir->mode = mode;
            dir->owner_uid = getuid();
            dir->group_gid = getgid();
            dir->inode = storageManager_->assignInode(path);
            dir->is_hidden = false;
            dir->is_system = false;

            storageManager_->cacheEntry(dir->inode, dir);
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

void FUSEBridge::forget(const fuse_req_t& req, const fuse_ino_t& ino, uint64_t nlookup) const {
    storageManager_->decrementInodeRef(ino, nlookup); // build this
    fuse_reply_none(req); // no return value
}

void FUSEBridge::access(const fuse_req_t& req, const fuse_ino_t& ino, int mask) const {
    fuse_reply_err(req, 0); // Access checks are not implemented, always allow
}

fuse_lowlevel_ops FUSEBridge::getOperations() const {
    fuse_lowlevel_ops ops = {};
    ops.getattr = FUSE_DISPATCH(getattr);
    ops.readdir = FUSE_DISPATCH(readdir);
    ops.lookup = FUSE_DISPATCH(lookup);
    ops.mkdir = FUSE_DISPATCH(mkdir);
    ops.open = FUSE_DISPATCH(open);
    ops.read = FUSE_DISPATCH(read);
    ops.forget = FUSE_DISPATCH(forget);
    ops.access = FUSE_DISPATCH(access);
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

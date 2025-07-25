#include "FUSEBridge.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "storage/StorageManager.hpp"
#include "types/File.hpp"
#include "types/FSEntry.hpp"
#include "types/Directory.hpp"
#include <boost/beast/core/file.hpp>
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

FUSEBridge::FUSEBridge(const std::shared_ptr<StorageManager>& storageManager)
    : storageManager_(storageManager) {}

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
        struct stat st = {};
        st.st_ino = storageManager_->getOrAssignInode(path / entry->name);
        st.st_mode = entry->isDirectory() ? S_IFDIR | 0755 : S_IFREG | 0644;
        st.st_size = static_cast<off_t>(entry->size_bytes);
        st.st_mtim.tv_sec = entry->updated_at;
        st.st_mtim.tv_nsec = 0; // Set nanoseconds to 0 for simplicity
        st.st_ctim = st.st_mtim; // Use the same time for ctime
        st.st_atim = st.st_mtim; // Use the same time for atime

        if (!add_entry(entry->name, st)) break;
    }

    fuse_reply_buf(req, buf.data(), buf_used);
}

fuse_lowlevel_ops FUSEBridge::getOperations() const {
    fuse_lowlevel_ops ops = {};
    ops.readdir = FUSE_DISPATCH(readdir);
    return ops;
}


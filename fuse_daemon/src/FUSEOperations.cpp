#include "../include/FUSEOperations.hpp"
#include "shared/StorageBridge/RemoteFSProxy.hpp"
#include "types/FileMetadata.hpp"
#include "types/File.hpp"

#include <cstring>
#include <cerrno>

static vh::shared::bridge::RemoteFSProxy* proxy = nullptr;

namespace vh::fuse {

    void bind(shared::bridge::RemoteFSProxy* p) {
        proxy = p;
    }

    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
        (void) fi;
        memset(stbuf, 0, sizeof(struct stat));

        if (!proxy->fileExists(path)) return -ENOENT;

        types::File file = proxy->stat(path);
        stbuf->st_mode = file.mode;
        stbuf->st_nlink = 1;
        stbuf->st_size = static_cast<long>(file.current_version_size_bytes);
        stbuf->st_mtime = file.updated_at;
        stbuf->st_ctime = file.created_at;
        stbuf->st_uid = getuid();
        stbuf->st_gid = getgid();
        return 0;
    }

    int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
                struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
        (void) offset;
        (void) fi;
        (void) flags;

        filler(buf, ".", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        filler(buf, "..", nullptr, 0, static_cast<fuse_fill_dir_flags>(0));

        auto entries = proxy->listDirectory(path);
        for (const auto& entry : entries) {
            filler(buf, entry.name.c_str(), nullptr, 0, static_cast<fuse_fill_dir_flags>(0));
        }

        return 0;
    }

    int open(const char* path, struct fuse_file_info* fi) {
        if (!proxy->fileExists(path)) return -ENOENT;
        return 0;
    }

    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
        (void) fi;
        return static_cast<int>(proxy->readFile(path, buf, size, offset));
    }

    struct fuse_operations getOperations() {
        struct fuse_operations ops = {};
        ops.getattr = getattr;
        ops.readdir = readdir;
        ops.open = open;
        ops.read = read;
        return ops;
    }

}

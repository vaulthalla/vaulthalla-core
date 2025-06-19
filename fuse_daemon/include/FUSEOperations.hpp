#pragma once

#define FUSE_USE_VERSION 35

#include <fuse3/fuse.h>

namespace vh::shared::bridge {
    class RemoteFSProxy;
}

namespace vh::fuse {

    void bind(shared::bridge::RemoteFSProxy* proxy);

    // FUSE callbacks
    int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);
    int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset,
                struct fuse_file_info* fi, enum fuse_readdir_flags flags);
    int open(const char* path, struct fuse_file_info* fi);
    int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);

    struct fuse_operations getOperations();

}

#pragma once

#define FUSE_USE_VERSION 35

#include <filesystem>
#include <fuse3/fuse_lowlevel.h>
#include <memory>

namespace vh::storage {
class StorageManager;
}

namespace vh::shared::bridge {
class RemoteFSProxy;
}

namespace vh::fuse {

class FUSEBridge {
public:
    explicit FUSEBridge(const std::shared_ptr<storage::StorageManager>& storageManager);

    void bind(std::shared_ptr<shared::bridge::RemoteFSProxy> p) const;

    int getattr(const char* path, struct stat* stbuf, fuse_file_info* fi) const;

    void readdir(const fuse_req_t& req, fuse_ino_t ino, size_t size, off_t off, fuse_file_info* fi) const;

    int open(const char* path, fuse_file_info* fi) const;

    int read(const char* path, char* buf, size_t size, off_t offset, fuse_file_info* fi) const;

    int write(const char* path, const char* buf, size_t size, off_t offset, fuse_file_info* fi) const;

    int create(const char* path, mode_t mode, fuse_file_info* fi) const;

    int unlink(const char* path) const;

    int truncate(const char* path, off_t size, fuse_file_info* fi) const;

    int rename(const char* from, const char* to, unsigned int flags) const;

    int mkdir(const char* path, mode_t mode) const;

    int rmdir(const char* path) const;

    int utimens(const char* path, const timespec tv[2], fuse_file_info* fi) const;

    int chmod(const char* path, mode_t mode, fuse_file_info* fi) const;

    int chown(const char* path, uid_t uid, gid_t gid, fuse_file_info* fi) const;

    int flush(const char* path, fuse_file_info* fi) const;

    int fsync(const char* path, int isdatasync, fuse_file_info* fi) const;

    int release(const char* path, fuse_file_info* fi) const;

    int access(const char* path, int mask) const;

    int statfs(const char* path, struct statvfs* stbuf) const;

    fuse_lowlevel_ops getOperations() const;

private:
    std::shared_ptr<storage::StorageManager> storageManager_;
};

}
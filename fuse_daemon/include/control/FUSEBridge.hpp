#pragma once

#define FUSE_USE_VERSION 35

#include <filesystem>
#include <fuse3/fuse_lowlevel.h>
#include <memory>

namespace vh::types {
struct FSEntry;
}

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

    void getattr(const fuse_req_t& req, const fuse_ino_t& ino, fuse_file_info* fi) const;

    void readdir(const fuse_req_t& req, fuse_ino_t ino, size_t size, off_t off, fuse_file_info* fi) const;

    void lookup(const fuse_req_t& req, const fuse_ino_t& parent, const char* name) const;

    void open(const fuse_req_t& req, const fuse_ino_t& ino, fuse_file_info* fi) const;

    void read(const fuse_req_t& req, const fuse_ino_t& ino, size_t size, off_t off, fuse_file_info* fi) const;

    void forget(const fuse_req_t& req, const fuse_ino_t& ino, uint64_t nlookup) const;

    void write(const char* path, const char* buf, size_t size, off_t offset, fuse_file_info* fi) const;

    void create(const fuse_req_t& req, fuse_ino_t parent, const char* name,
                        mode_t mode, struct fuse_file_info* fi);

    void unlink(const char* path) const;

    void truncate(const char* path, off_t size, fuse_file_info* fi) const;

    void rename(const fuse_req_t& req,
                        fuse_ino_t parent,
                        const char* name,
                        fuse_ino_t newparent,
                        const char* newname,
                        unsigned int flags) const;

    void mkdir(const fuse_req_t& req, const fuse_ino_t& parent, const char* name, mode_t mode) const;

    void rmdir(const char* path) const;

    void utimens(const char* path, const timespec tv[2], fuse_file_info* fi) const;

    void chmod(const char* path, mode_t mode, fuse_file_info* fi) const;

    void chown(const char* path, uid_t uid, gid_t gid, fuse_file_info* fi) const;

    void flush(const char* path, fuse_file_info* fi) const;

    void fsync(const char* path, int isdatasync, fuse_file_info* fi) const;

    void release(const char* path, fuse_file_info* fi) const;

    void access(const fuse_req_t& req, const fuse_ino_t& ino, int mask) const;

    void statfs(const char* path, struct statvfs* stbuf) const;

    fuse_lowlevel_ops getOperations() const;

private:
    std::shared_ptr<storage::StorageManager> storageManager_;

    struct stat statFromEntry(const std::shared_ptr<types::FSEntry>& entry, const fuse_ino_t& ino) const;
};

}
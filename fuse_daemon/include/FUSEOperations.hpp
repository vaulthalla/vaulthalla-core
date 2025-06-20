#pragma once

#define FUSE_USE_VERSION 35

#include <fuse3/fuse.h>
#include <memory>

namespace vh::shared::bridge {
class RemoteFSProxy;
}

namespace vh::fuse {

class FUSEPermissions;

void bindPermissions(std::unique_ptr<FUSEPermissions> perms);
void bind(std::shared_ptr<shared::bridge::RemoteFSProxy> p);

// Core file operations
int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi);
int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi,
            enum fuse_readdir_flags flags);
int open(const char* path, struct fuse_file_info* fi);
int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi);
int create(const char* path, mode_t mode, struct fuse_file_info* fi);
int unlink(const char* path);
int truncate(const char* path, off_t size, struct fuse_file_info* fi);
int rename(const char* from, const char* to, unsigned int flags);
int mkdir(const char* path, mode_t mode);
int rmdir(const char* path);
int utimens(const char* path, const struct timespec tv[2], struct fuse_file_info* fi);
int chmod(const char* path, mode_t mode, struct fuse_file_info* fi);
int chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi);
int flush(const char* path, struct fuse_file_info* fi);
int fsync(const char* path, int isdatasync, struct fuse_file_info* fi);
int release(const char* path, struct fuse_file_info* fi);

// Optional: extended attributes, access checks, etc.
int access(const char* path, int mask);
int statfs(const char* path, struct statvfs* stbuf);

// Expose FUSE operation struct
struct fuse_operations getOperations();

} // namespace vh::fuse

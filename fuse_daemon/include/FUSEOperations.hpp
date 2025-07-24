#pragma once

#define FUSE_USE_VERSION 35

#include <fuse3/fuse_lowlevel.h>
#include <memory>

namespace vh::shared::bridge {
class RemoteFSProxy;
}

namespace vh::fuse {

class FUSEPermissions;

void bindPermissions(std::unique_ptr<FUSEPermissions> perms);

void bind(std::shared_ptr<shared::bridge::RemoteFSProxy> p);

int getattr(const char* path, struct stat* stbuf, fuse_file_info* fi);

void readdir(const fuse_req_t& req, fuse_ino_t ino, size_t size, off_t off, fuse_file_info* fi);

int open(const char* path, fuse_file_info* fi);

int read(const char* path, char* buf, size_t size, off_t offset, fuse_file_info* fi);

int write(const char* path, const char* buf, size_t size, off_t offset, fuse_file_info* fi);

int create(const char* path, mode_t mode, fuse_file_info* fi);

int unlink(const char* path);

int truncate(const char* path, off_t size, fuse_file_info* fi);

int rename(const char* from, const char* to, unsigned int flags);

int mkdir(const char* path, mode_t mode);

int rmdir(const char* path);

int utimens(const char* path, const timespec tv[2], fuse_file_info* fi);

int chmod(const char* path, mode_t mode, fuse_file_info* fi);

int chown(const char* path, uid_t uid, gid_t gid, fuse_file_info* fi);

int flush(const char* path, fuse_file_info* fi);

int fsync(const char* path, int isdatasync, fuse_file_info* fi);

int release(const char* path, fuse_file_info* fi);

int access(const char* path, int mask);

int statfs(const char* path, struct statvfs* stbuf);

fuse_lowlevel_ops getOperations();

}
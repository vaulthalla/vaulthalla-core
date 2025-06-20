#include "FUSEOperations.hpp"
#include "../../shared/include/types/db/File.hpp"
#include "FUSEPermissions.hpp"
#include "StorageBridge/RemoteFSProxy.hpp"
#include "types/db/FileMetadata.hpp"
#include <boost/beast/core/file.hpp>
#include <cerrno>
#include <cstring>
#include <sys/statvfs.h>
#include <unistd.h>

static vh::shared::bridge::RemoteFSProxy* proxy = nullptr;

namespace vh::fuse {

static std::shared_ptr<shared::bridge::RemoteFSProxy> proxy = nullptr;

void bind(std::shared_ptr<shared::bridge::RemoteFSProxy> p) {
    proxy = std::move(p);
}

static std::unique_ptr<FUSEPermissions> permissions = nullptr;

void bindPermissions(std::unique_ptr<FUSEPermissions> perms) {
    permissions = std::move(perms);
}

int getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    (void)fi;
    memset(stbuf, 0, sizeof(struct stat));

    if (!proxy->fileExists(path)) return -ENOENT;

    types::File file = proxy->stat(path);
    stbuf->st_mode = file.mode;
    stbuf->st_nlink = 1;
    stbuf->st_size = static_cast<off_t>(file.current_version_size_bytes);
    stbuf->st_mtime = file.updated_at;
    stbuf->st_ctime = file.created_at;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();

    return 0;
}

int readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info* fi,
            enum fuse_readdir_flags flags) {
    (void)offset;
    (void)fi;
    (void)flags;

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

    auto file = proxy->stat(path);
    const auto* ctx = fuse_get_context();

    if (!permissions->hasPermission(file, ctx->uid, ctx->gid, R_OK)) return -EACCES;
    return 0;
}

int read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void)fi;

    auto file = proxy->stat(path);
    const auto* ctx = fuse_get_context();

    if (!permissions->hasPermission(file, ctx->uid, ctx->gid, R_OK)) return -EACCES;
    return static_cast<int>(proxy->readFile(path, buf, size, offset));
}

int write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    (void)fi;

    auto file = proxy->stat(path);
    const auto* ctx = fuse_get_context();

    if (!permissions->hasPermission(file, ctx->uid, ctx->gid, W_OK)) return -EACCES;
    return static_cast<int>(proxy->writeFile(path, buf, size, offset));
}

int create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    const auto* ctx = fuse_get_context();
    return proxy->createFile(path, mode /*, ctx->uid, ctx->gid */) ? 0 : -EIO;
}

int unlink(const char* path) {
    auto file = proxy->stat(path);
    const auto* ctx = fuse_get_context();

    if (!permissions->hasPermission(file, ctx->uid, ctx->gid, W_OK)) return -EACCES;
    return proxy->deleteFile(path) ? 0 : -ENOENT;
}

int truncate(const char* path, off_t size, struct fuse_file_info* fi) {
    (void)fi;
    return proxy->resizeFile(path, static_cast<size_t>(size)) ? 0 : -EIO;
}

int rename(const char* from, const char* to, unsigned int flags) {
    (void)flags;
    return proxy->rename(from, to) ? 0 : -EIO;
}

int mkdir(const char* path, mode_t mode) {
    return proxy->mkdir(path, mode) ? 0 : -EIO;
}

int rmdir(const char* path) {
    return proxy->deleteDirectory(path) ? 0 : -ENOTEMPTY;
}

int utimens(const char* path, const struct timespec tv[2], struct fuse_file_info* fi) {
    (void)fi;
    return proxy->updateTimestamps(path, tv[0].tv_sec, tv[1].tv_sec) ? 0 : -EIO;
}

int chmod(const char* path, mode_t mode, struct fuse_file_info* fi) {
    (void)fi;
    return proxy->setPermissions(path, mode) ? 0 : -EIO;
}

int chown(const char* path, uid_t uid, gid_t gid, struct fuse_file_info* fi) {
    (void)fi;
    return proxy->setOwnership(path, uid, gid) ? 0 : -EIO;
}

int flush(const char* path, struct fuse_file_info* fi) {
    (void)path;
    (void)fi;
    return 0;
}

int fsync(const char* path, int isdatasync, struct fuse_file_info* fi) {
    (void)path;
    (void)isdatasync;
    (void)fi;
    return 0;
}

int release(const char* path, struct fuse_file_info* fi) {
    (void)path;
    (void)fi;
    return 0;
}

int access(const char* path, int mask) {
    if (!proxy->fileExists(path)) return -ENOENT;

    auto file = proxy->stat(path);
    const auto* ctx = fuse_get_context();

    if (!permissions->hasPermission(file, ctx->uid, ctx->gid, mask)) return -EACCES;
    return 0;
}

int statfs(const char* path, struct statvfs* stbuf) {
    (void)path;
    memset(stbuf, 0, sizeof(struct statvfs));
    stbuf->f_bsize = 4096;
    stbuf->f_blocks = proxy->getTotalBlocks();
    stbuf->f_bfree = proxy->getFreeBlocks();
    return 0;
}

struct fuse_operations getOperations() {
    struct fuse_operations ops = {};
    ops.getattr = getattr;
    ops.readdir = readdir;
    ops.open = open;
    ops.read = read;
    ops.write = write;
    ops.create = create;
    ops.unlink = unlink;
    ops.truncate = truncate;
    ops.rename = rename;
    ops.mkdir = mkdir;
    ops.rmdir = rmdir;
    ops.utimens = utimens;
    ops.chmod = chmod;
    ops.chown = chown;
    ops.flush = flush;
    ops.fsync = fsync;
    ops.release = release;
    ops.access = access;
    ops.statfs = statfs;
    return ops;
}

} // namespace vh::fuse

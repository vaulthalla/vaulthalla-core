#include "FUSEOperations.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "types/File.hpp"
#include "types/FileMetadata.hpp"
#include <boost/beast/core/file.hpp>
#include <cerrno>
#include <cstring>
#include <sys/statvfs.h>
#include <unistd.h>

using namespace vh::database;

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
    stbuf->st_mode = file.isDirectory() ? S_IFDIR | 0755 : S_IFREG | 0644;
    stbuf->st_nlink = 1;
    stbuf->st_size = static_cast<off_t>(file.size_bytes);
    stbuf->st_mtime = file.updated_at;
    stbuf->st_ctime = file.created_at;
    stbuf->st_uid = getuid();
    stbuf->st_gid = getgid();

    return 0;
}

void readdir(const fuse_req_t& req, fuse_ino_t ino, const size_t size, const off_t off, const fuse_file_info* fi) {
    (void)fi;

    std::string path = resolvePathFromInode(ino); // implement this to map inode → path
    auto entries = DirectoryQueries::listDir()

    std::vector<char> buf(size);
    size_t buf_used = 0;

    auto add_entry = [&](const std::string& name, const struct stat& st) {
        size_t entry_size = fuse_add_direntry(req, nullptr, 0, name.c_str(), &st, 0);
        if (buf_used + entry_size > size) return false;

        fuse_add_direntry(req, buf.data() + buf_used, entry_size, name.c_str(), &st, buf_used);
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

    for (const auto& entry : *entries) {
        struct stat st = {};
        st.st_ino = entry.inode;
        st.st_mode = entry.is_dir ? S_IFDIR | 0755 : S_IFREG | 0644;
        st.st_size = entry.size;
        st.st_mtim.tv_sec = entry.mtime;

        if (!add_entry(entry.name, st)) break;
    }

    fuse_reply_buf(req, buf.data(), buf_used);
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
    struct fuse_low ops = {};
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

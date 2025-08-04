#include "types/Path.hpp"
#include "util/fsPath.hpp"
#include "config/ConfigRegistry.hpp"

using namespace vh::types;
using namespace vh::config;

Path::Path(const fs::path& vaultMountPoint)
    : fuseRoot(ConfigRegistry::get().fuse.root_mount_path),
      vaultRoot(fuseRoot / stripLeadingSlash(vaultMountPoint)),
      cacheRoot(ConfigRegistry::get().fuse.backing_path /
                stripLeadingSlash(ConfigRegistry::get().caching.path) /
                stripLeadingSlash(vaultMountPoint)),
      thumbnailRoot(cacheRoot / "thumbnails"),
      fileCacheRoot(cacheRoot / "files"),
      backingRoot(ConfigRegistry::get().fuse.backing_path),
      backingVaultRoot(ConfigRegistry::get().fuse.backing_path /
                       stripLeadingSlash(vaultMountPoint)) {}

fs::path Path::absPath(const fs::path& relPath, const PathType& type) const {
    switch (type) {
    case PathType::FUSE_ROOT:
        return fuseRoot / stripLeadingSlash(relPath);
    case PathType::VAULT_ROOT:
        return vaultRoot / stripLeadingSlash(relPath);
    case PathType::CACHE_ROOT:
        return cacheRoot / stripLeadingSlash(relPath);
    case PathType::THUMBNAIL_ROOT:
        return thumbnailRoot / stripLeadingSlash(relPath);
    case PathType::FILE_CACHE_ROOT:
        return fileCacheRoot / stripLeadingSlash(relPath);
    case PathType::BACKING_ROOT:
        return backingRoot / stripLeadingSlash(relPath);
    case PathType::BACKING_VAULT_ROOT:
        return backingVaultRoot / stripLeadingSlash(relPath);
    default:
        throw std::invalid_argument("Invalid PathType");
    }
}

fs::path Path::relPath(const fs::path& absPath, const PathType& type) const {
    switch (type) {
    case PathType::FUSE_ROOT:
        return absPath.lexically_relative(fuseRoot).make_preferred();
    case PathType::VAULT_ROOT:
        return absPath.lexically_relative(vaultRoot).make_preferred();
    case PathType::CACHE_ROOT:
        return absPath.lexically_relative(cacheRoot).make_preferred();
    case PathType::THUMBNAIL_ROOT:
        return absPath.lexically_relative(thumbnailRoot).make_preferred();
    case PathType::FILE_CACHE_ROOT:
        return absPath.lexically_relative(fileCacheRoot).make_preferred();
    case PathType::BACKING_ROOT:
        return absPath.lexically_relative(backingRoot).make_preferred();
    case PathType::BACKING_VAULT_ROOT:
        return absPath.lexically_relative(backingVaultRoot).make_preferred();
    default:
        throw std::invalid_argument("Invalid PathType");
    }
}

fs::path Path::absRelToRoot(const fs::path& path, const PathType& type) const {
    const auto rel = relPath(path, type);
    if (rel.empty() || rel.string().starts_with("..")) return makeAbsolute(path.filename());
    return makeAbsolute(rel.lexically_normal());
}

fs::path Path::absRelToAbsRel(const fs::path& path, const PathType& initial, const PathType& target) const {
    return absRelToRoot(absPath(path, initial), target);
}

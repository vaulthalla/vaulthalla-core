#include "types/Path.hpp"
#include "util/fsPath.hpp"
#include "config/ConfigRegistry.hpp"
#include "services/LogRegistry.hpp"

#include <paths.h>

using namespace vh::types;
using namespace vh::config;
using namespace vh::logging;

Path::Path(const fs::path& vaultFuseMount, const fs::path& vaultBackingMount)
    : fuseRoot(paths::getMountPath()),
      vaultRoot(fuseRoot / stripLeadingSlash(vaultFuseMount)),
      cacheRoot(paths::getBackingPath() /
                stripLeadingSlash(paths::getCachePath()) /
                stripLeadingSlash(vaultBackingMount)),
      thumbnailRoot(cacheRoot / "thumbnails"),
      fileCacheRoot(cacheRoot / "files"),
      backingRoot(paths::getBackingPath()),
      backingVaultRoot(paths::getBackingPath() /
                       stripLeadingSlash(vaultBackingMount)) {

    LogRegistry::storage()->debug("[Path] Initialized paths:\nfuseRoot: {}\nvaultRoot: {}\n"
                                  "cacheRoot: {}\nthumbnailRoot: {}\nfileCacheRoot: {}\n"
                                  "backingRoot: {}\nbackingVaultRoot: {}",
                                  fuseRoot.string(), vaultRoot.string(), cacheRoot.string(),
                                  thumbnailRoot.string(), fileCacheRoot.string(),
                                  backingRoot.string(), backingVaultRoot.string());
}

fs::path Path::absPath(const fs::path& relPath, const PathType& type) const {
    switch (type) {
    case PathType::FUSE_ROOT:
        if (relPath.string() == "/") return fuseRoot;
        return fuseRoot / stripLeadingSlash(relPath);
    case PathType::VAULT_ROOT:
        if (relPath.string() == "/") return vaultRoot;
        return vaultRoot / stripLeadingSlash(relPath);
    case PathType::CACHE_ROOT:
        if (relPath.string() == "/") return cacheRoot;
        return cacheRoot / stripLeadingSlash(relPath);
    case PathType::THUMBNAIL_ROOT:
        if (relPath.string() == "/") return thumbnailRoot;
        return thumbnailRoot / stripLeadingSlash(relPath);
    case PathType::FILE_CACHE_ROOT:
        if (relPath.string() == "/") return fileCacheRoot;
        return fileCacheRoot / stripLeadingSlash(relPath);
    case PathType::BACKING_ROOT:
        if (relPath.string() == "/") return backingRoot;
        return backingRoot / stripLeadingSlash(relPath);
    case PathType::BACKING_VAULT_ROOT:
        if (relPath.string() == "/") return backingVaultRoot;
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

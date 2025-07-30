#pragma once

#include "util/fsPath.hpp"
#include "config/ConfigRegistry.hpp"
#include "database/Queries/FSEntryQueries.hpp"
#include "storage/Filesystem.hpp"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

using namespace vh::config;
using namespace vh::database;

namespace vh::types {

enum class PathType {
    FUSE_ROOT,
    VAULT_ROOT,
    CACHE_ROOT,
    THUMBNAIL_ROOT,
    FILE_CACHE_ROOT,
    BACKING_ROOT
};

struct Path {
    fs::path fuseRoot, vaultRoot, cacheRoot, thumbnailRoot, fileCacheRoot, backingRoot;

    explicit Path(const fs::path& vaultMountPoint) {
        const auto& conf = ConfigRegistry::get();
        fuseRoot = conf.fuse.root_mount_path;
        vaultRoot = fuseRoot / stripLeadingSlash(vaultMountPoint);
        cacheRoot = conf.fuse.backing_path / stripLeadingSlash(conf.caching.path) / stripLeadingSlash(vaultMountPoint);
        thumbnailRoot = cacheRoot / "thumbnails";
        fileCacheRoot = cacheRoot / "files";
        backingRoot = conf.fuse.backing_path / stripLeadingSlash(vaultMountPoint);
    }

    [[nodiscard]] fs::path absPath(const fs::path& relPath, const PathType& type) const {
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
            default:
                throw std::invalid_argument("Invalid PathType");
        }
    }

    [[nodiscard]] fs::path relPath(const fs::path& absPath, const PathType& type) const {
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
            default:
                throw std::invalid_argument("Invalid PathType");
        }
    }

    [[nodiscard]] fs::path absRelToRoot(const fs::path& path, const PathType& type) const {
        auto rel = relPath(path, type);
        if (rel.empty() || rel.string().starts_with("..")) return makeAbsolute(path.filename());
        return makeAbsolute(rel.lexically_normal());
    }
};

}

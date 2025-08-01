#pragma once

#include "config/ConfigRegistry.hpp"
#include "database/Queries/FSEntryQueries.hpp"

#include <filesystem>

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
    BACKING_ROOT,
    BACKING_VAULT_ROOT
};

struct Path {
    fs::path fuseRoot, vaultRoot, cacheRoot, thumbnailRoot, fileCacheRoot, backingRoot, backingVaultRoot;

    explicit Path(const fs::path& vaultMountPoint);

    [[nodiscard]] fs::path absPath(const fs::path& relPath, const PathType& type) const;
    [[nodiscard]] fs::path relPath(const fs::path& absPath, const PathType& type) const;
    [[nodiscard]] fs::path absRelToRoot(const fs::path& path, const PathType& type) const;
    [[nodiscard]] fs::path absRelToAbsOther(const fs::path& path, const PathType& initial, const PathType& target) const;
};

}

#pragma once

#include <filesystem>

namespace fs = std::filesystem;

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
    const fs::path fuseRoot, vaultRoot, cacheRoot, thumbnailRoot, fileCacheRoot, backingRoot, backingVaultRoot;

    explicit Path(const fs::path& vaultFuseMount, const fs::path& vaultBackingMount);

    [[nodiscard]] fs::path absPath(const fs::path& relPath, const PathType& type) const;
    [[nodiscard]] fs::path relPath(const fs::path& absPath, const PathType& type) const;

    /// Converts an absolute or relative path to an absolute path relative to the root of the specified type.
    [[nodiscard]] fs::path absRelToRoot(const fs::path& path, const PathType& type) const;

    /// Converts a path from one type to another, preserving the relative structure.
    /// For example, converting from FUSE_ROOT to VAULT_ROOT.
    /// e.g. absRelToAbsRel("/users/admin/vault1/test.txt", PathType::FUSE_ROOT, PathType::VAULT_ROOT)
    /// return "/vault1/test.txt"
    [[nodiscard]] fs::path absRelToAbsRel(const fs::path& path, const PathType& initial, const PathType& target) const;
};

}

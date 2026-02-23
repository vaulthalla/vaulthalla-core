#pragma once

#include <filesystem>

namespace vh::fs::model {

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
    const std::filesystem::path fuseRoot, vaultRoot, cacheRoot, thumbnailRoot, fileCacheRoot, backingRoot, backingVaultRoot;

    explicit Path(const std::filesystem::path& vaultFuseMount, const std::filesystem::path& vaultBackingMount);

    [[nodiscard]] std::filesystem::path absPath(const std::filesystem::path& relPath, const PathType& type) const;
    [[nodiscard]] std::filesystem::path relPath(const std::filesystem::path& absPath, const PathType& type) const;

    /// Converts an absolute or relative path to an absolute path relative to the root of the specified type.
    [[nodiscard]] std::filesystem::path absRelToRoot(const std::filesystem::path& path, const PathType& type) const;

    /// Converts a path from one type to another, preserving the relative structure.
    /// For example, converting from FUSE_ROOT to VAULT_ROOT.
    /// e.g. absRelToAbsRel("/users/admin/vault1/test.txt", PathType::FUSE_ROOT, PathType::VAULT_ROOT)
    /// return "/vault1/test.txt"
    [[nodiscard]] std::filesystem::path absRelToAbsRel(const std::filesystem::path& path, const PathType& initial, const PathType& target) const;
};

}

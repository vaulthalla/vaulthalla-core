#pragma once

#include "types/CacheIndex.hpp"

#include <memory>
#include <filesystem>
#include <vector>
#include <optional>

namespace vh::database {

struct CacheQueries {
    CacheQueries() = default;

    static void addCacheIndex(const std::shared_ptr<types::CacheIndex>& index);
    static void upsertCacheIndex(const std::shared_ptr<types::CacheIndex>& index);
    static void updateCacheIndex(const std::shared_ptr<types::CacheIndex>& index);
    static void deleteCacheIndex(unsigned int indexId);
    static void deleteCacheIndex(unsigned int vaultId, const std::filesystem::path& relPath);
    static std::shared_ptr<types::CacheIndex> getCacheIndex(unsigned int indexId);
    static std::shared_ptr<types::CacheIndex> getCacheIndexByPath(unsigned int vaultId, const std::filesystem::path& path);
    static std::vector<std::shared_ptr<types::CacheIndex>> listCacheIndices(unsigned int vaultId, const std::filesystem::path& relPath = {}, bool recursive = false);
    static std::vector<std::shared_ptr<types::CacheIndex>> listCacheIndicesByFile(unsigned int fileId);
    [[nodiscard]] static std::vector<std::shared_ptr<types::CacheIndex>> listCacheIndicesByType(unsigned int vaultId, const types::CacheIndex::Type& type);
    [[nodiscard]] static std::vector<std::shared_ptr<types::CacheIndex>> nLargestCacheIndices(unsigned int n, unsigned int vaultId, const std::filesystem::path& relPath, bool recursive = false);
    [[nodiscard]] static std::vector<std::shared_ptr<types::CacheIndex>> nLargestCacheIndicesByType(unsigned int n, unsigned int vaultId, const types::CacheIndex::Type& type);

    [[nodiscard]] static unsigned int countCacheIndices(unsigned int vaultId, const std::optional<types::CacheIndex::Type>& type = std::nullopt);

    [[nodiscard]] static bool cacheIndexExists(unsigned int vaultId, const std::filesystem::path& relPath);
};

}

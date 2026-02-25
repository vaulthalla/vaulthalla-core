#pragma once

#include "fs/cache/Record.hpp"

#include <memory>
#include <filesystem>
#include <vector>
#include <optional>

namespace vh::database {

struct CacheQueries {
    CacheQueries() = default;

    static void addCacheIndex(const std::shared_ptr<fs::cache::Record>& index);
    static void upsertCacheIndex(const std::shared_ptr<fs::cache::Record>& index);
    static void updateCacheIndex(const std::shared_ptr<fs::cache::Record>& index);
    static void deleteCacheIndex(unsigned int indexId);
    static void deleteCacheIndex(unsigned int vaultId, const std::filesystem::path& relPath);
    static std::shared_ptr<fs::cache::Record> getCacheIndex(unsigned int indexId);
    static std::shared_ptr<fs::cache::Record> getCacheIndexByPath(unsigned int vaultId, const std::filesystem::path& path);
    static std::vector<std::shared_ptr<fs::cache::Record>> listCacheIndices(unsigned int vaultId, const std::filesystem::path& relPath = {}, bool recursive = false);
    static std::vector<std::shared_ptr<fs::cache::Record>> listCacheIndicesByFile(unsigned int fileId);
    [[nodiscard]] static std::vector<std::shared_ptr<fs::cache::Record>> listCacheIndicesByType(unsigned int vaultId, const fs::cache::Record::Type& type);
    [[nodiscard]] static std::vector<std::shared_ptr<fs::cache::Record>> nLargestCacheIndices(unsigned int n, unsigned int vaultId, const std::filesystem::path& relPath, bool recursive = false);
    [[nodiscard]] static std::vector<std::shared_ptr<fs::cache::Record>> nLargestCacheIndicesByType(unsigned int n, unsigned int vaultId, const fs::cache::Record::Type& type);

    [[nodiscard]] static unsigned int countCacheIndices(unsigned int vaultId, const std::optional<fs::cache::Record::Type>& type = std::nullopt);

    [[nodiscard]] static bool cacheIndexExists(unsigned int vaultId, const std::filesystem::path& relPath);
};

}

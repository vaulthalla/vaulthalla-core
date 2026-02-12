#include "types/stats/CapacityStats.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "types/fs/Directory.hpp"
#include "types/fs/File.hpp"
#include "types/vault/Vault.hpp"
#include "storage/StorageEngine.hpp"
#include "storage/StorageManager.hpp"
#include "database/Queries/FileQueries.hpp"
#include "services/ServiceDepsRegistry.hpp"

#include <nlohmann/json.hpp>

using namespace vh::types;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::util;

struct CompareFiles {
    bool operator()(const std::pair<std::string, unsigned int>& a, const std::pair<std::string, unsigned int>& b) const {
        return a.second < b.second;
    }
};

static std::array<std::pair<std::string, uintmax_t>, 10>
toTop10Array(const std::vector<ExtensionStat>& v) {
    std::array<std::pair<std::string, uintmax_t>, 10> a{};
    for (size_t i = 0; i < a.size() && i < v.size(); ++i) {
        a[i] = { v[i].extension, static_cast<uintmax_t>(v[i].total_bytes) };
    }
    return a;
}

static nlohmann::json top10ToDict(const std::array<std::pair<std::string, uintmax_t>, 10>& arr) {
    nlohmann::json j;
    for (const auto & [k, v] : arr) j[k] = v;
    return j;
}

CapacityStats::CapacityStats(const unsigned int vaultId) {
    const auto engine = services::ServiceDepsRegistry::instance().storageManager->getEngine(vaultId);
    if (!engine) throw std::runtime_error("vaultId not available");

    const auto dir = DirectoryQueries::getDirectoryByPath(vaultId, "/");
    if (!dir) throw std::runtime_error("vaultId not found");

    capacity = engine->vault->quota;
    logical_size = dir->size_bytes;
    physical_size = engine->getVaultSize();
    cache_size = engine->getCacheSize();
    free_space = engine->freeSpace();
    file_count = dir->file_count;
    directory_count = dir->subdirectory_count;
    average_file_size_bytes = file_count > 0 ? dir->size_bytes / file_count : 0;
    if (const auto largest = FileQueries::getLargestFile(engine->vault->id))
        largest_file_size_bytes = largest->size_bytes;
    top_file_extensions_by_size =
        toTop10Array(FileQueries::getTopExtensionsBySize(engine->vault->id, 10));
}

void vh::types::to_json(nlohmann::json& j, const std::shared_ptr<CapacityStats>& stats) {
    j["capacity"] = stats->capacity;
    j["logical_size"] = stats->logical_size;
    j["physical_size"] = stats->physical_size;
    j["free_space"] = stats->free_space;
    j["cache_size"] = stats->cache_size;
    j["file_count"] = stats->file_count;
    j["directory_count"] = stats->directory_count;
    j["average_file_size"] = stats->average_file_size_bytes;
    j["largest_file_size"] = stats->largest_file_size_bytes;
    j["top_file_extensions"] = top10ToDict(stats->top_file_extensions_by_size);
}

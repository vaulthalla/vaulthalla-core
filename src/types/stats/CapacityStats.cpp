#include "types/stats/CapacityStats.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "types/Directory.hpp"
#include "types/File.hpp"
#include "types/Vault.hpp"
#include "storage/StorageEngine.hpp"
#include "storage/StorageManager.hpp"
#include "database/Queries/FileQueries.hpp"
#include "services/ServiceDepsRegistry.hpp"

using namespace vh::types;
using namespace vh::storage;
using namespace vh::database;

struct CompareFiles {
    static bool operator()(const std::pair<std::string, unsigned int>& a, const std::pair<std::string, unsigned int>& b) const {
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

CapacityStats::CapacityStats(const unsigned int vaultId) {
    const auto engine = services::ServiceDepsRegistry::instance().storageManager->getEngine(vaultId);
    if (!engine) throw std::runtime_error("vaultId not available");

    const auto dir = DirectoryQueries::getDirectoryByPath(vaultId, "/");
    if (!dir) throw std::runtime_error("vaultId not found");

    capacity = engine->vault->quota;
    logical_size = dir->size_bytes;
    physical_size = engine->getDirectorySize("/");
    file_count = dir->file_count;
    directory_count = dir->subdirectory_count;
    average_file_size_bytes = dir->size_bytes / file_count;
    if (const auto largest = FileQueries::getLargestFile(engine->vault->id))
        largest_file_size_bytes = largest->size_bytes;
    top_file_extensions_by_size =
        toTop10Array(FileQueries::getTopExtensionsBySize(engine->vault->id, 10));
}

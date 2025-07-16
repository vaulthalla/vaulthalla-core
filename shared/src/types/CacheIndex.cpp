#include "types/CacheIndex.hpp"
#include "shared_util/timestamp.hpp"

#include <pqxx/result>
#include <nlohmann/json.hpp>

using namespace vh::types;

CacheIndex::CacheIndex(const pqxx::row& row)
    : id(row.at("id").as<unsigned int>()),
      vault_id(row.at("vault_id").as<unsigned int>()),
      file_id(row.at("file_id").as<unsigned int>()),
      path(row.at("path").as<std::string>()),
      type(typeFromString(row.at("type").as<std::string>())),
      size(row.at("size").as<uintmax_t>()),
      last_accessed(util::parsePostgresTimestamp(row.at("last_accessed").as<std::string>())),
      created_at(util::parsePostgresTimestamp(row.at("created_at").as<std::string>())) {}

void vh::types::to_json(nlohmann::json& j, const CacheIndex& index) {
    j = {
        {"id", index.id},
        {"vault_id", index.vault_id},
        {"file_id", index.file_id},
        {"path", index.path.string()},
        {"type", to_string(index.type)},
        {"size", index.size},
        {"last_accessed", util::timestampToString(index.last_accessed)},
        {"created_at", util::timestampToString(index.created_at)}
    };
}

void vh::types::from_json(const nlohmann::json& j, CacheIndex& index) {
    index.id = j.value("id", 0);
    index.vault_id = j.value("vault_id", 0);
    index.file_id = j.value("file_id", 0);
    index.path = j.value("path", std::filesystem::path{});
    index.type = typeFromString(j.value("type", "file"));
    index.size = j.value("size", 0u);
    index.last_accessed = util::parseTimestampFromString(j.value("last_accessed", ""));
    index.created_at = util::parseTimestampFromString(j.value("created_at", ""));
}

std::string vh::types::to_string(const CacheIndex::Type& type) {
    switch (type) {
        case CacheIndex::Type::File: return "file";
        case CacheIndex::Type::Thumbnail: return "thumbnail";
        default: throw std::invalid_argument("Unknown CacheIndex type");
    }
}

CacheIndex::Type vh::types::typeFromString(const std::string& str) {
    if (str == "file") return CacheIndex::Type::File;
    if (str == "thumbnail") return CacheIndex::Type::Thumbnail;
    throw std::invalid_argument("Unknown CacheIndex type: " + str);
}

std::vector<std::shared_ptr<CacheIndex>> vh::types::cache_indices_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<CacheIndex>> indexes;
    indexes.reserve(res.size());
    for (const auto& row : res) indexes.emplace_back(std::make_shared<CacheIndex>(row));
    return indexes;
}

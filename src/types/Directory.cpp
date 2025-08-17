#include "types/Directory.hpp"
#include "util/timestamp.hpp"
#include "database/Queries/FSEntryQueries.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/result>

using namespace vh::types;
using namespace vh::database;

Directory::Directory(const pqxx::row& row, const pqxx::result& parentRows)
    : FSEntry(row, parentRows),
      file_count(row["file_count"].as<unsigned int>()),
      subdirectory_count(row["subdirectory_count"].as<unsigned int>()),
      last_modified(util::parsePostgresTimestamp(row["last_modified"].as<std::string>())) {}

void vh::types::to_json(nlohmann::json& j, const Directory& d) {
    to_json(j, static_cast<const FSEntry&>(d));
    j["type"] = "directory"; // Helpful for client
    j["file_count"] = d.file_count;
    j["subdirectory_count"] = d.subdirectory_count;
    j["last_modified"] = util::timestampToString(d.last_modified);
}

void vh::types::from_json(const nlohmann::json& j, Directory& d) {
    from_json(j, static_cast<FSEntry&>(d));
    d.file_count = j.at("file_count").get<unsigned int>();
    d.subdirectory_count = j.at("subdirectory_count").get<unsigned int>();
    d.last_modified = util::parsePostgresTimestamp(j.at("last_modified").get<std::string>());
}

std::vector<std::shared_ptr<Directory>> vh::types::directories_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<Directory>> directories;
    for (const auto& row : res) {
        if (const auto parentId = row["parent_id"].as<std::optional<unsigned int>>()) {
            const auto parentChain = FSEntryQueries::collectParentChain(*parentId);
            directories.push_back(std::make_shared<Directory>(row, parentChain));
        } else directories.push_back(std::make_shared<Directory>(row, pqxx::result{}));
    }
    return directories;
}

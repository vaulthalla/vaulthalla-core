#include "types/Directory.hpp"
#include "util/timestamp.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <pqxx/result>

using namespace vh::types;

DirectoryStats::DirectoryStats(const pqxx::row& row) {
    file_count = row["file_count"].as<unsigned int>();
    subdirectory_count = row["subdirectory_count"].as<unsigned int>();
    size_bytes = row["size_bytes"].as<unsigned long long>();
    last_modified = util::parsePostgresTimestamp(row["last_modified"].as<std::string>());
}

Directory::Directory(const pqxx::row& row) : FSEntry(row), stats(std::make_shared<DirectoryStats>(row)) {}

void vh::types::to_json(nlohmann::json& j, const DirectoryStats& stats) {
    j = {
        {"size_bytes", stats.size_bytes},
        {"file_count", stats.file_count},
        {"subdirectory_count", stats.subdirectory_count},
        {"last_modified", vh::util::timestampToString(stats.last_modified)}
    };
}

void vh::types::from_json(const nlohmann::json& j, DirectoryStats& stats) {
    stats.size_bytes = j.at("size_bytes").get<unsigned long long>();
    stats.file_count = j.at("file_count").get<unsigned int>();
    stats.subdirectory_count = j.at("subdirectory_count").get<unsigned int>();
    stats.last_modified = vh::util::parsePostgresTimestamp(j.at("last_modified").get<std::string>());
}

void vh::types::to_json(nlohmann::json& j, const Directory& d) {
    to_json(j, static_cast<const FSEntry&>(d));
    j["stats"] = *d.stats;
    j["type"] = "directory"; // Helpful for client
}

void vh::types::from_json(const nlohmann::json& j, Directory& d) {
    from_json(j, static_cast<FSEntry&>(d));
    d.stats = std::make_shared<DirectoryStats>(j.at("stats").get<DirectoryStats>());
}

std::vector<std::shared_ptr<Directory>> vh::types::directories_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<Directory>> directories;
    for (const auto& row : res) directories.push_back(std::make_shared<Directory>(row));
    return directories;
}

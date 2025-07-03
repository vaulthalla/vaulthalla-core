#include "types/FSEntry.hpp"
#include "util/timestamp.hpp"
#include "types/Directory.hpp"
#include "types/File.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>

using namespace vh::types;

FSEntry::FSEntry(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      vault_id(row["vault_id"].as<unsigned int>()),
      created_by(row["created_by"].as<unsigned int>()),
      last_modified_by(row["last_modified_by"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(util::parsePostgresTimestamp(row["updated_at"].as<std::string>())),
      path(std::filesystem::path(row["path"].as<std::string>())) {
    if (row["parent_id"].is_null()) parent_id = std::nullopt;
    else parent_id = row["parent_id"].as<unsigned int>();
}

void vh::types::to_json(nlohmann::json& j, const FSEntry& entry) {
    j = {
        {"id", entry.id},
        {"vault_id", entry.vault_id},
        {"created_by", entry.created_by},
        {"last_modified_by", entry.last_modified_by},
        {"name", entry.name},
        {"created_at", util::timestampToString(entry.created_at)},
        {"updated_at", util::timestampToString(entry.updated_at)},
        {"path", entry.path.string()}
    };
}

void vh::types::from_json(const nlohmann::json& j, FSEntry& entry) {
    entry.id = j.at("id").get<unsigned int>();
    entry.vault_id = j.at("vault_id").get<unsigned int>();
    entry.created_by = j.at("created_by").get<unsigned int>();
    entry.last_modified_by = j.at("last_modified_by").get<unsigned int>();
    entry.name = j.at("name").get<std::string>();
    entry.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
    entry.updated_at = util::parsePostgresTimestamp(j.at("updated_at").get<std::string>());
    entry.path = std::filesystem::path(j.at("path").get<std::string>());

    if (j.contains("parent_id") && !j["parent_id"].is_null()) entry.parent_id = j.at("parent_id").get<unsigned int>();
    else entry.parent_id.reset();
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<FSEntry>>& entries) {
    j = nlohmann::json::array();
    for (const auto& entry : entries) {
        if (entry->isDirectory()) j.push_back(*std::static_pointer_cast<Directory>(entry));
        else j.push_back(*std::static_pointer_cast<File>(entry));
    }
}

std::vector<std::shared_ptr<FSEntry> > vh::types::merge_entries(
    const std::vector<std::shared_ptr<File>>& files,
    const std::vector<std::shared_ptr<Directory>>& directories) {
    std::vector<std::shared_ptr<FSEntry>> entries;
    entries.reserve(files.size() + directories.size());

    entries.insert(entries.end(), files.begin(), files.end());
    entries.insert(entries.end(), directories.begin(), directories.end());

    return entries;
}
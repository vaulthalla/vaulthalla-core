#include "types/db/File.hpp"
#include "util/timestamp.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>

namespace vh::types {

File::File(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      storage_volume_id(row["storage_volume_id"].as<unsigned int>()),
      parent_id(row["parent_id"].is_null() ? std::nullopt : std::make_optional(row["parent_id"].as<unsigned int>())),
      name(row["name"].as<std::string>()),
      is_directory(row["is_directory"].as<bool>()),
      mode(row["mode"].as<unsigned long long>()),
      uid(row["uid"].as<unsigned int>()),
      gid(row["gid"].as<unsigned int>()),
      created_by(row["created_by"].as<unsigned int>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(util::parsePostgresTimestamp(row["updated_at"].as<std::string>())),
      current_version_size_bytes(row["current_version_size_bytes"].as<unsigned long long>()),
      is_trashed(row["is_trashed"].as<bool>()),
      trashed_at(util::parsePostgresTimestamp(row["trashed_at"].as<std::string>())),
      trashed_by(row["trashed_by"].as<unsigned int>()) {
    if (!row["full_path"].is_null()) full_path = row["full_path"].as<std::string>();
    else full_path.reset();
}

void to_json(nlohmann::json& j, const File& f) {
    j = {
        {"id", f.id},
        {"storage_volume_id", f.storage_volume_id},
        {"parent_id", f.parent_id},
        {"name", f.name},
        {"is_directory", f.is_directory},
        {"mode", f.mode},
        {"uid", f.uid},
        {"gid", f.gid},
        {"created_by", f.created_by},
        {"created_at", util::timestampToString(f.created_at)},
        {"updated_at", util::timestampToString(f.updated_at)},
        {"current_version_size_bytes", f.current_version_size_bytes},
        {"is_trashed", f.is_trashed},
        {"trashed_at", util::timestampToString(f.trashed_at)},
        {"trashed_by", f.trashed_by},
        {"full_path", f.full_path}
    };
}

void from_json(const nlohmann::json& j, File& f) {
    f.id = j.at("id").get<unsigned int>();
    f.storage_volume_id = j.at("storage_volume_id").get<unsigned int>();
    f.parent_id = j.contains("parent_id") && !j["parent_id"].is_null()
                      ? std::make_optional(j.at("parent_id").get<unsigned int>())
                      : std::nullopt;
    f.name = j.at("name").get<std::string>();
    f.is_directory = j.at("is_directory").get<bool>();
    f.mode = j.at("mode").get<unsigned long long>();
    f.uid = j.at("uid").get<unsigned int>();
    f.gid = j.at("gid").get<unsigned int>();
    f.created_by = j.at("created_by").get<unsigned int>();
    f.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
    f.updated_at = util::parsePostgresTimestamp(j.at("updated_at").get<std::string>());
    f.current_version_size_bytes = j.at("current_version_size_bytes").get<unsigned long long>();
    f.is_trashed = j.at("is_trashed").get<bool>();
    f.trashed_at = util::parsePostgresTimestamp(j.at("trashed_at").get<std::string>());
    f.trashed_by = j.at("trashed_by").get<unsigned int>();

    if (j.contains("full_path") && !j["full_path"].is_null()) f.full_path = j.at("full_path").get<std::string>();
    else f.full_path.reset();
}

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<File>>& files) {
    j = nlohmann::json::array();
    for (const auto& file : files) j.push_back(*file);
}

}

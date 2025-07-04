#include "types/Role.hpp"
#include "types/PermissionOverride.hpp"
#include "util/timestamp.hpp"

#include <pqxx/row>
#include <pqxx/result>
#include <nlohmann/json.hpp>

using namespace vh::types;

Role::Role(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      simplePermissions(row["simple_permissions"].as<bool>()) {
    if (simplePermissions) file_permissions = directory_permissions = static_cast<uint16_t>(row["permissions"].as<int64_t>());
    else {
        file_permissions = static_cast<uint16_t>(row["file_permissions"].as<int64_t>());
        directory_permissions = static_cast<uint16_t>(row["directory_permissions"].as<int64_t>());
    }
}

Role::Role(const nlohmann::json& j)
    : id(j.contains("id") ? j.at("id").get<unsigned int>() : 0),
      name(j.at("name").get<std::string>()),
      description(j.at("description").get<std::string>()),
      created_at(static_cast<std::time_t>(0)),
      simplePermissions(j.value("simple_permissions", false)) {
    if (simplePermissions) file_permissions = directory_permissions = fsMaskFromJson(j.at("permissions"));
    else {
        file_permissions = fsMaskFromJson(j.at("file_permissions"));
        directory_permissions = fsMaskFromJson(j.at("directory_permissions"));
    }
}

void vh::types::to_json(nlohmann::json& j, const Role& r) {
    j = {
        {"id", r.id},
        {"name", r.name},
        {"description", r.description},
        {"simple_permissions", r.simplePermissions},
        {"created_at", util::timestampToString(r.created_at)}
    };

    if (r.simplePermissions) j["permissions"] = jsonFromFSMask(r.file_permissions);
    else {
        j["file_permissions"] = jsonFromFSMask(r.file_permissions);
        j["directory_permissions"] = jsonFromFSMask(r.directory_permissions);
    }
}

void vh::types::from_json(const nlohmann::json& j, Role& r) {
    if (j.contains("id")) r.id = j.at("id").get<unsigned int>();
    r.name = j.at("name").get<std::string>();
    r.description = j.at("description").get<std::string>();
    r.simplePermissions = j.value("simple_permissions", false);
    r.file_permissions = fsMaskFromJson(j.at("file_permissions"));
    r.directory_permissions = fsMaskFromJson(j.at("directory_permissions"));
    r.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role> >& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
}

std::vector<std::shared_ptr<Role> > vh::types::roles_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<Role> > roles;
    for (const auto& item : res) roles.push_back(std::make_shared<Role>(item));
    return roles;
}

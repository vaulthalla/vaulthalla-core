#include "types/db/BaseRole.hpp"
#include "util/timestamp.hpp"
#include "types/db/Permission.hpp"

#include <nlohmann/json.hpp>

using namespace vh::types;

BaseRole::BaseRole(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      display_name(row["display_name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      permissions(row["permissions"].as<uint16_t>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())) {}

BaseRole::BaseRole(const nlohmann::json& j)
    : id(j.at("id").get<unsigned int>()),
      name(j.at("name").get<std::string>()),
      description(j.at("description").get<std::string>()),
      permissions(toBitmask(j.at("permissions").get<std::vector<PermissionName> >())),
      created_at(util::parsePostgresTimestamp(j.at("created_at").get<std::string>())) {}

void vh::types::to_json(nlohmann::json& j, const BaseRole& r) {
    j = {
        {"id", r.id},
        {"name", r.name},
        {"display_name", r.display_name},
        {"description", r.description},
        {"permissions", permsFromBitmaskAsString(r.permissions)},
        {"created_at", util::timestampToString(r.created_at)},
    };
}

void vh::types::from_json(const nlohmann::json& j, BaseRole& r) {
    r.id = j.at("id").get<unsigned int>();
    r.name = j.at("name").get<std::string>();
    r.description = j.at("description").get<std::string>();
    r.permissions = toBitmask(j.at("permissions").get<std::vector<PermissionName> >());
    r.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
}

std::vector<std::shared_ptr<BaseRole>> vh::types::roles_from_json(const nlohmann::json& j) {
    std::vector<std::shared_ptr<BaseRole>> roles;
    for (const auto& roleJson : j) roles.push_back(std::make_shared<BaseRole>(roleJson));
    return roles;
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<BaseRole>>& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
}

nlohmann::json from_json(const std::vector<std::shared_ptr<BaseRole>>& roles) {
    nlohmann::json j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
    return j;
}

bool BaseRole::canManageUsers() const { return hasPermission(permissions, PermissionName::ManageUsers); }
bool BaseRole::canManageRoles() const { return hasPermission(permissions, PermissionName::ManageRoles); }
bool BaseRole::canManageStorage() const { return hasPermission(permissions, PermissionName::ManageStorage); }
bool BaseRole::canManageFiles() const { return hasPermission(permissions, PermissionName::ManageFiles); }
bool BaseRole::canViewAuditLog() const { return hasPermission(permissions, PermissionName::ViewAuditLog); }
bool BaseRole::canUploadFile() const { return hasPermission(permissions, PermissionName::UploadFile); }
bool BaseRole::canDownloadFile() const { return hasPermission(permissions, PermissionName::DownloadFile); }
bool BaseRole::canDeleteFile() const { return hasPermission(permissions, PermissionName::DeleteFile); }
bool BaseRole::canShareFile() const { return hasPermission(permissions, PermissionName::ShareFile); }
bool BaseRole::canLockFile() const { return hasPermission(permissions, PermissionName::LockFile); }
bool BaseRole::canManageSettings() const { return hasPermission(permissions, PermissionName::ManageSettings); }

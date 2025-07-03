#include "types/Role.hpp"
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
        {"created_at", util::timestampToString(r.created_at)},
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
    r.file_permissions = fsMaskFromJson(j.at("file_permissions"));
    r.directory_permissions = fsMaskFromJson(j.at("directory_permissions"));
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

bool Role::canUploadFile() const { return hasPermission(file_permissions, FSPermission::Upload); }
bool Role::canDownloadFile() const { return hasPermission(file_permissions, FSPermission::Download); }
bool Role::canDeleteFile() const { return hasPermission(file_permissions, FSPermission::Delete); }
bool Role::canShareFilePublicly() const { return hasPermission(file_permissions, FSPermission::SharePublic); }
bool Role::canShareFileInternally() const { return hasPermission(file_permissions, FSPermission::ShareInternal); }
bool Role::canLockFile() const { return hasPermission(file_permissions, FSPermission::Lock); }
bool Role::canRenameFile() const { return hasPermission(file_permissions, FSPermission::Rename); }
bool Role::canMoveFile() const { return hasPermission(file_permissions, FSPermission::Move); }
bool Role::canSyncFileLocally() const { return hasPermission(file_permissions, FSPermission::SyncLocal); }
bool Role::canSyncFileWithCloud() const { return hasPermission(file_permissions, FSPermission::SyncCloud); }
bool Role::canManageFileMetadata() const { return hasPermission(file_permissions, FSPermission::ModifyMetadata); }
bool Role::canChangeFileIcons() const { return hasPermission(file_permissions, FSPermission::ChangeIcons); }
bool Role::canManageVersions() const { return hasPermission(file_permissions, FSPermission::ManageVersions); }
bool Role::canManageFileTags() const { return hasPermission(file_permissions, FSPermission::ManageTags); }

bool Role::canUploadDirectory() const { return hasPermission(directory_permissions, FSPermission::Upload); }
bool Role::canDownloadDirectory() const { return hasPermission(directory_permissions, FSPermission::Download); }
bool Role::canDeleteDirectory() const { return hasPermission(directory_permissions, FSPermission::Delete); }
bool Role::canShareDirPublicly() const { return hasPermission(directory_permissions, FSPermission::SharePublic); }
bool Role::canShareDirInternally() const { return hasPermission(directory_permissions, FSPermission::ShareInternal); }
bool Role::canLockDirectory() const { return hasPermission(directory_permissions, FSPermission::Lock); }
bool Role::canRenameDirectory() const { return hasPermission(directory_permissions, FSPermission::Rename); }
bool Role::canMoveDirectory() const { return hasPermission(directory_permissions, FSPermission::Move); }
bool Role::canSyncDirectoryLocally() const { return hasPermission(directory_permissions, FSPermission::SyncLocal); }
bool Role::canSyncDirectoryWithCloud() const { return hasPermission(directory_permissions, FSPermission::SyncCloud); }
bool Role::canManageDirectoryMetadata() const { return hasPermission(directory_permissions, FSPermission::ModifyMetadata); }
bool Role::canChangeDirectoryIcons() const { return hasPermission(directory_permissions, FSPermission::ChangeIcons); }
bool Role::canManageDirectoryTags() const { return hasPermission(directory_permissions, FSPermission::ManageTags); }
bool Role::canListDirectory() const { return hasPermission(directory_permissions, FSPermission::List); }

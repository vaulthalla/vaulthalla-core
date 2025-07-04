#include "types/AssignedRole.hpp"
#include "types/PermissionOverride.hpp"
#include "util/timestamp.hpp"

#include <pqxx/row>
#include <pqxx/result>
#include <nlohmann/json.hpp>

using namespace vh::types;

AssignedRole::AssignedRole(const pqxx::row& row, const pqxx::result& overrides)
    : Role(row),
      id(row["id"].as<unsigned int>()),
      subject_id(row["subject_id"].as<unsigned int>()),
      role_id(row["role_id"].as<unsigned int>()),
      vault_id(row["vault_id"].as<unsigned int>()),
      subject_type(row["subject_type"].as<std::string>()),
      assigned_at(util::parsePostgresTimestamp(row["assigned_at"].as<std::string>())),
      permission_overrides(permissionOverridesFromPqRes(overrides)) {
}

AssignedRole::AssignedRole(const pqxx::row& row, const std::vector<pqxx::row>& overrides)
    : Role(row),
      id(row["id"].as<unsigned int>()),
      subject_id(row["subject_id"].as<unsigned int>()),
      role_id(row["role_id"].as<unsigned int>()),
      vault_id(row["vault_id"].as<unsigned int>()),
      subject_type(row["subject_type"].as<std::string>()),
      assigned_at(util::parsePostgresTimestamp(row["assigned_at"].as<std::string>())) {
    for (const auto& override : overrides) permission_overrides.push_back(
        std::make_shared<PermissionOverride>(override));
}

AssignedRole::AssignedRole(const nlohmann::json& j)
    : Role(j),
      id(j.at("id").get<unsigned int>()),
      subject_id(j.at("subject_id").get<unsigned int>()),
      role_id(j.at("role_id").get<unsigned int>()),
      vault_id(j.at("vault_id").get<unsigned int>()),
      subject_type(j.at("subject_type").get<std::string>()),
      assigned_at(util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>())),
      permission_overrides(j.at("permission_overrides")) {
}

void vh::types::to_json(nlohmann::json& j, const AssignedRole& r) {
    to_json(j, static_cast<const Role&>(r)); // fill base Role part first
    j.update({
        {"id", r.id},
        {"vault_id", r.vault_id},
        {"subject_type", r.subject_type},
        {"subject_id", r.subject_id},
        {"role_id", r.role_id},
        {"assigned_at", util::timestampToString(r.assigned_at)},
        {"permission_overrides", r.permission_overrides}
    });
}

void vh::types::from_json(const nlohmann::json& j, AssignedRole& r) {
    from_json(j, static_cast<Role&>(r));
    r.id = j.at("id").get<unsigned int>();
    r.vault_id = j.at("vault_id").get<unsigned int>();
    r.subject_type = j.at("subject_type").get<std::string>();
    r.subject_id = j.at("subject_id").get<unsigned int>();
    r.role_id = j.at("role_id").get<unsigned int>();
    r.assigned_at = util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>());
    r.permission_overrides = permissionOverridesFromJson(j.at("permission_overrides"));
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<AssignedRole> >& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
}

std::vector<std::shared_ptr<AssignedRole> > vh::types::assigned_roles_from_json(const nlohmann::json& j) {
    std::vector<std::shared_ptr<AssignedRole> > roles;
    for (const auto& roleJson : j) roles.push_back(std::make_shared<AssignedRole>(roleJson));
    return roles;
}

std::vector<std::shared_ptr<AssignedRole> > vh::types::assigned_roles_from_pq_result(
    const pqxx::result& res,
    const pqxx::result& overrides
    ) {
    std::vector<std::shared_ptr<AssignedRole> > roles;

    std::unordered_map<unsigned int, std::vector<pqxx::row> > overrideMap;
    for (const auto& overrideRow : overrides) {
        unsigned int roleId = overrideRow["role_id"].as<unsigned int>();
        overrideMap[roleId].push_back(overrideRow);
    }

    for (const auto& item : res) {
        unsigned int roleId = item["id"].as<unsigned int>();
        const auto& roleOverrides = overrideMap[roleId];
        roles.push_back(std::make_shared<AssignedRole>(item, roleOverrides));
    }

    return roles;
}


std::vector<std::shared_ptr<PermissionOverride> > AssignedRole::getPermissionOverrides(const unsigned short bit, const bool isFile) const {
    std::vector<std::shared_ptr<PermissionOverride>> overrides;
    for (const auto& override : permission_overrides)
        if (override->permission.bit_position == bit && override->is_file == isFile) overrides.push_back(override);
    return overrides;
}


// FILE permissions
bool AssignedRole::canUploadFile(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::Upload, path, true);
}
bool AssignedRole::canDownloadFile(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::Download, path, true);
}
bool AssignedRole::canDeleteFile(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::Delete, path, true);
}
bool AssignedRole::canShareFilePublicly(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::SharePublic, path, true);
}
bool AssignedRole::canShareFileInternally(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::ShareInternal, path, true);
}
bool AssignedRole::canLockFile(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::Lock, path, true);
}
bool AssignedRole::canRenameFile(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::Rename, path, true);
}
bool AssignedRole::canMoveFile(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::Move, path, true);
}
bool AssignedRole::canSyncFileLocally(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::SyncLocal, path, true);
}
bool AssignedRole::canSyncFileWithCloud(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::SyncCloud, path, true);
}
bool AssignedRole::canManageFileMetadata(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::ModifyMetadata, path, true);
}
bool AssignedRole::canChangeFileIcons(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::ChangeIcons, path, true);
}
bool AssignedRole::canManageVersions(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::ManageVersions, path, true);
}
bool AssignedRole::canManageFileTags(const std::filesystem::path& path) const {
    return validatePermission(file_permissions, FSPermission::ManageTags, path, true);
}

// DIRECTORY permissions
bool AssignedRole::canUploadDirectory(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::Upload, path, false);
}
bool AssignedRole::canDownloadDirectory(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::Download, path, false);
}
bool AssignedRole::canDeleteDirectory(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::Delete, path, false);
}
bool AssignedRole::canShareDirPublicly(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::SharePublic, path, false);
}
bool AssignedRole::canShareDirInternally(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::ShareInternal, path, false);
}
bool AssignedRole::canLockDirectory(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::Lock, path, false);
}
bool AssignedRole::canRenameDirectory(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::Rename, path, false);
}
bool AssignedRole::canMoveDirectory(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::Move, path, false);
}
bool AssignedRole::canSyncDirectoryLocally(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::SyncLocal, path, false);
}
bool AssignedRole::canSyncDirectoryWithCloud(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::SyncCloud, path, false);
}
bool AssignedRole::canManageDirectoryMetadata(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::ModifyMetadata, path, false);
}
bool AssignedRole::canChangeDirectoryIcons(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::ChangeIcons, path, false);
}
bool AssignedRole::canManageDirectoryTags(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::ManageTags, path, false);
}
bool AssignedRole::canListDirectory(const std::filesystem::path& path) const {
    return validatePermission(directory_permissions, FSPermission::List, path, false);
}

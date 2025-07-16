#include "types/LocalDiskVault.hpp"
#include "shared_util/timestamp.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>

namespace vh::types {

LocalDiskVault::LocalDiskVault(const std::string& name, std::filesystem::path mountPoint)
    : Vault(), mount_point(std::move(mountPoint)) {
    this->name = name;
    this->type = VaultType::Local;
    this->is_active = true;
    this->created_at = std::time(nullptr);
}

LocalDiskVault::LocalDiskVault(const pqxx::row& row)
    : Vault(row),
      mount_point(std::filesystem::path(row["mount_point"].as<std::string>())) {}

void to_json(nlohmann::json& j, const LocalDiskVault& v) {
    to_json(j, static_cast<const Vault&>(v));
    j["mount_point"] = v.mount_point.string();
}

void from_json(const nlohmann::json& j, LocalDiskVault& v) {
    from_json(j, static_cast<Vault&>(v));
    v.mount_point = j.at("mount_point").get<std::string>();
}

}

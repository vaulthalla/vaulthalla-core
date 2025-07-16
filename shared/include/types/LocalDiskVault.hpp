#pragma once

#include "Vault.hpp"
#include <filesystem>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
}

namespace vh::types {

struct LocalDiskVault : Vault {
    std::filesystem::path mount_point;

    LocalDiskVault() = default;
    LocalDiskVault(const std::string& name, std::filesystem::path mountPoint);
    explicit LocalDiskVault(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const LocalDiskVault& v);
void from_json(const nlohmann::json& j, LocalDiskVault& v);

}

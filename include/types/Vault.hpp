#pragma once

#include <ctime>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>
#include <filesystem>

namespace pqxx {
class row;
}

namespace vh::types {

enum class VaultType { Local, S3 };

std::string to_string(VaultType type);
VaultType from_string(const std::string& type);

struct Vault {
    unsigned int id{};
    unsigned int owner_id{};
    std::string name, description{};
    unsigned long long quota{};
    VaultType type{VaultType::Local};
    std::filesystem::path mount_point;
    bool is_active{true};
    std::time_t created_at{};

    Vault() = default;
    virtual ~Vault() = default;

    explicit Vault(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const Vault& v);
void from_json(const nlohmann::json& j, Vault& v);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Vault>>& vaults);

}

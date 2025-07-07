#pragma once

#include <ctime>
#include <memory>
#include <string>
#include <vector>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>

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
    VaultType type{VaultType::Local};
    bool is_active{true};
    std::time_t created_at{};

    Vault() = default;
    virtual ~Vault() = default;

    explicit Vault(const pqxx::row& row);
};

struct LocalDiskVault : Vault {
    unsigned int vault_id{};
    std::filesystem::path mount_point;

    LocalDiskVault() = default;
    LocalDiskVault(const std::string& name, std::filesystem::path mountPoint);
    explicit LocalDiskVault(const pqxx::row& row);
};

struct S3Vault : Vault {
    unsigned short vault_id{};
    unsigned short api_key_id{};
    std::string bucket;

    S3Vault() = default;
    S3Vault(const std::string& name, unsigned short apiKeyID, std::string bucketName);
    explicit S3Vault(const pqxx::row& row);
};

// JSON serialization
void to_json(nlohmann::json& j, const Vault& v);
void from_json(const nlohmann::json& j, Vault& v);

void to_json(nlohmann::json& j, const LocalDiskVault& v);
void from_json(const nlohmann::json& j, LocalDiskVault& v);

void to_json(nlohmann::json& j, const S3Vault& v);
void from_json(const nlohmann::json& j, S3Vault& v);

nlohmann::json to_json(const std::vector<std::shared_ptr<Vault>>& vaults);

} // namespace vh::types

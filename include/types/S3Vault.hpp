#pragma once

#include "Vault.hpp"
#include <string>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
}

namespace vh::types {

struct S3Vault : Vault {
    unsigned int api_key_id{};
    std::string bucket;

    S3Vault() = default;
    S3Vault(const std::string& name, unsigned int apiKeyID, std::string bucketName);
    explicit S3Vault(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const S3Vault& v);
void from_json(const nlohmann::json& j, S3Vault& v);

}

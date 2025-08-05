#pragma once

#include <memory>
#include <string>
#include <ctime>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
}

namespace vh::types::api {

struct APIKey;

struct S3Bucket {
    std::unique_ptr<APIKey> api_key;
    std::string name;
    std::time_t created_at, updated_at;
    bool enabled{true};

    explicit S3Bucket(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const S3Bucket& bucket);
void from_json(const nlohmann::json& j, S3Bucket& bucket);

}

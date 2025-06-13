#pragma once

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <pqxx/row>
#include <utility>
#include <nlohmann/json.hpp>

namespace vh::types {

    struct APIKey {
        unsigned int id{0};
        unsigned int user_id{0};
        std::string name;
        std::time_t created_at{std::time(nullptr)};

        APIKey() = default;
        virtual ~APIKey() = default;

        APIKey(unsigned int id, unsigned int userId, std::string name, std::time_t createdAt)
            : id(id), user_id(userId), name(std::move(name)), created_at(createdAt) {}
    };

    struct S3APIKey : public APIKey {
        std::string access_key;
        std::string secret_access_key;
        std::string region;
        std::string endpoint;

        S3APIKey() = default;

        S3APIKey(const std::string& name,
                 unsigned int userId,
                 std::string  accessKey,
                 std::string  secretAccessKey,
                 std::string  region,
                 std::string  endpoint)
            : APIKey{0, userId, name, std::time(nullptr)}, // ID and user_id will be set by the database
              access_key(std::move(accessKey)),
              secret_access_key(std::move(secretAccessKey)),
              region(std::move(region)),
              endpoint(std::move(endpoint)) {}

        explicit S3APIKey(const pqxx::row& row)
            : APIKey{row["id"].as<unsigned int>(),
                     row["user_id"].as<unsigned int>(),
                     row["name"].as<std::string>(),
                     row["created_at"].as<std::time_t>()},
              access_key(row["access_key"].as<std::string>()),
              secret_access_key(row["secret_access_key"].as<std::string>()),
              region(row["region"].as<std::string>()),
              endpoint(row["endpoint"].as<std::string>()) {}

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(vh::types::S3APIKey,
                                       id,
                                       user_id,
                                       name,
                                       created_at,
                                       access_key,
                                       secret_access_key,
                                       region,
                                       endpoint)
    };

    inline nlohmann::json to_json(const std::shared_ptr<APIKey>& key) {
        nlohmann::json key_json = {
                {"id", key->id},
                {"user_id", key->user_id},
                {"name", key->name},
                {"created_at", key->created_at}
        };
        if (auto s3Key = std::dynamic_pointer_cast<S3APIKey>(key)) {
            key_json["access_key"] = s3Key->access_key;
            key_json["secret_access_key"] = s3Key->secret_access_key;
            key_json["region"] = s3Key->region;
            key_json["endpoint"] = s3Key->endpoint;
        }
        return key_json;
    }

    inline nlohmann::json to_json(const std::vector<std::shared_ptr<APIKey>>& keys) {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& key : keys) j.push_back(to_json(key));
        return j;
    }

}

BOOST_DESCRIBE_STRUCT(vh::types::APIKey, (), (id, user_id, name, created_at))

BOOST_DESCRIBE_STRUCT(vh::types::S3APIKey, (vh::types::APIKey), (access_key, secret_access_key, region, endpoint))

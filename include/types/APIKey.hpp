#pragma once

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <pqxx/row>

namespace vh::types {

    struct APIKey {
        unsigned int id;
        unsigned int user_id;
        std::string name;
        std::time_t created_at;
    };

    struct S3APIKey : public APIKey {
        std::string access_key;
        std::string secret_access_key;
        std::string region;
        std::string endpoint;

        S3APIKey(const pqxx::row& row)
            : APIKey{row["id"].as<unsigned int>(),
                     row["user_id"].as<unsigned int>(),
                     row["name"].as<std::string>(),
                     row["created_at"].as<std::time_t>()},
              access_key(row["access_key"].as<std::string>()),
              secret_access_key(row["secret_access_key"].as<std::string>()),
              region(row["region"].as<std::string>()),
              endpoint(row["endpoint"].as<std::string>()) {}
    };

}

BOOST_DESCRIBE_STRUCT(vh::types::APIKey, (), (id, user_id, name, created_at))

BOOST_DESCRIBE_STRUCT(vh::types::S3APIKey, (vh::types::APIKey), (access_key, secret_access_key, region, endpoint))

#include "database/Queries/APIKeyQueries.hpp"
#include "database/Transactions.hpp"

namespace vh::database {
unsigned int APIKeyQueries::addAPIKey(const std::shared_ptr<types::api::APIKey>& key) {
    return Transactions::exec("APIKeyQueries::addAPIKey", [&](pqxx::work& txn) {
        pqxx::result res =
            txn.exec("INSERT INTO api_keys (user_id, type, name) VALUES (" + txn.quote(key->user_id) + ", " +
                     txn.quote(types::api::to_string(key->type)) + ", " + txn.quote(key->name) + ") RETURNING id");

        auto keyId = res[0][0].as<unsigned int>();

        if (auto s3Key = std::dynamic_pointer_cast<types::api::S3APIKey>(key)) {
            txn.exec("INSERT INTO s3_api_keys (api_key_id, provider, access_key, secret_access_key, region, endpoint) "
                     "VALUES (" +
                     txn.quote(keyId) + ", " + txn.quote(types::api::to_string(s3Key->provider)) + ", " +
                     txn.quote(s3Key->access_key) + ", " + txn.quote(s3Key->secret_access_key) + ", " +
                     txn.quote(s3Key->region) + ", " + txn.quote(s3Key->endpoint) + ")");
        }

        txn.commit();

        return keyId;
    });
}

void APIKeyQueries::removeAPIKey(unsigned int keyId) {
    Transactions::exec("APIKeyQueries::removeAPIKey", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM s3_api_keys WHERE api_key_id = " + txn.quote(keyId));
        txn.exec("DELETE FROM api_keys WHERE id = " + txn.quote(keyId));
        txn.commit();
    });
}

std::vector<std::shared_ptr<types::api::APIKey> > APIKeyQueries::listAPIKeys(unsigned int userId) {
    return Transactions::exec("APIKeyQueries::listAPIKeys", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec("SELECT api_keys.*, s3_api_keys.provider "
                                    "FROM api_keys "
                                    "LEFT JOIN s3_api_keys ON api_keys.id = s3_api_keys.api_key_id "
                                    "WHERE api_keys.user_id = " +
                                    txn.quote(userId));

        std::vector<std::shared_ptr<types::api::APIKey> > keys;
        for (const auto& row : res) keys.push_back(std::make_shared<types::api::APIKey>(row));

        return keys;
    });
}

std::vector<std::shared_ptr<types::api::APIKey> > APIKeyQueries::listAPIKeys() {
    return Transactions::exec("APIKeyQueries::listAPIKeys", [&](pqxx::work& txn) {
        pqxx::result res = txn.exec("SELECT api_keys.*, s3_api_keys.provider "
            "FROM api_keys "
            "LEFT JOIN s3_api_keys ON api_keys.id = s3_api_keys.api_key_id");

        std::vector<std::shared_ptr<types::api::APIKey> > keys;
        for (const auto& row : res) keys.push_back(std::make_shared<types::api::APIKey>(row));

        return keys;
    });
}

std::shared_ptr<types::api::APIKey> APIKeyQueries::getAPIKey(unsigned int keyId) {
    return Transactions::exec("APIKeyQueries::getAPIKey", [&](pqxx::work& txn) {
        pqxx::row row = txn.exec("SELECT type FROM api_keys WHERE id = " + txn.quote(keyId)).one_row();

        auto type = row["type"].as<std::string>();

        if (type == "s3") {
            pqxx::result res = txn.exec("SELECT * FROM api_keys "
                                        "JOIN s3_api_keys ON api_keys.id = s3_api_keys.api_key_id "
                                        "WHERE api_keys.id = " +
                                        txn.quote(keyId));
            if (res.empty()) throw std::runtime_error("API key not found with ID: " + std::to_string(keyId));
            return std::make_shared<types::api::S3APIKey>(res[0]);
        } else {
            throw std::runtime_error("Unsupported API key type: " + type);
        }
    });
}
} // namespace vh::database
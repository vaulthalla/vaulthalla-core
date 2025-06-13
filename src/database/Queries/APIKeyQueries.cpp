#include "database/Queries/APIKeyQueries.hpp"
#include "database/Transactions.hpp"

namespace vh::database {
    void APIKeyQueries::addAPIKey(const std::shared_ptr<vh::types::APIKey> &key) {
        Transactions::exec("APIKeyQueries::addAPIKey", [&](pqxx::work& txn) {
            txn.exec("INSERT INTO api_keys (id, user_id, name, created_at) VALUES ("
                     + txn.quote(key->id) + ", "
                     + txn.quote(key->user_id) + ", "
                     + txn.quote(key->name) + ", "
                     + txn.quote(key->created_at) + ")");

            if (auto s3Key = std::dynamic_pointer_cast<vh::types::S3APIKey>(key)) {
                txn.exec("INSERT INTO s3_api_keys (api_key_id, access_key, secret_access_key, region, endpoint) VALUES ("
                         + txn.quote(s3Key->id) + ", "
                         + txn.quote(s3Key->access_key) + ", "
                         + txn.quote(s3Key->secret_access_key) + ", "
                         + txn.quote(s3Key->region) + ", "
                         + txn.quote(s3Key->endpoint) + ")");
            }

            txn.commit();
        });
    }

    void APIKeyQueries::removeAPIKey(unsigned int keyId) {
        Transactions::exec("APIKeyQueries::removeAPIKey", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM s3_api_keys WHERE api_key_id = " + txn.quote(keyId));
            txn.exec("DELETE FROM api_keys WHERE id = " + txn.quote(keyId));
            txn.commit();
        });
    }

    std::vector<std::shared_ptr<vh::types::APIKey>> APIKeyQueries::listAPIKeys(unsigned int userId) {
        return Transactions::exec("APIKeyQueries::listAPIKeys", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM api_keys WHERE user_id = " + txn.quote(userId));
            std::vector<std::shared_ptr<vh::types::APIKey>> keys;

            for (const auto& row : res) {
                if (row["type"].as<std::string>() == "s3") {
                    auto s3Key = std::make_shared<vh::types::S3APIKey>(row);
                    keys.push_back(s3Key);
                } else throw std::runtime_error("Unsupported API key type: " + row["type"].as<std::string>());
            }

            return keys;
        });
    }

    std::vector<std::shared_ptr<vh::types::APIKey>> APIKeyQueries::listAPIKeys() {
        return Transactions::exec("APIKeyQueries::listAPIKeys", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM api_keys");
            std::vector<std::shared_ptr<vh::types::APIKey>> keys;

            for (const auto& row : res) {
                if (row["type"].as<std::string>() == "s3") {
                    auto s3Key = std::make_shared<vh::types::S3APIKey>(row);
                    keys.push_back(s3Key);
                } else throw std::runtime_error("Unsupported API key type: " + row["type"].as<std::string>());
            }

            return keys;
        });
    }

    std::shared_ptr<vh::types::APIKey> APIKeyQueries::getAPIKey(unsigned int keyId) {
        return Transactions::exec("APIKeyQueries::getAPIKey", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM api_keys WHERE id = " + txn.quote(keyId));
            if (res.empty()) throw std::runtime_error("API key not found with ID: " + std::to_string(keyId));

            const auto& row = res[0];
            if (row["type"].as<std::string>() == "s3") return std::make_shared<vh::types::S3APIKey>(row);
            else throw std::runtime_error("Unsupported API key type: " + row["type"].as<std::string>());
        });
    }
}

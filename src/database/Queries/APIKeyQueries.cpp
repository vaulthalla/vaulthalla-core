#include "database/Queries/APIKeyQueries.hpp"
#include "database/Transactions.hpp"

namespace vh::database {
    void APIKeyQueries::addS3APIKey(const vh::types::S3APIKey &key) {
        Transactions::exec("APIKeyQueries::addS3APIKey", [&](pqxx::work& txn) {
            txn.exec("INSERT INTO api_keys (id, user_id, name, created_at) VALUES ("
                     + txn.quote(key.id) + ", "
                     + txn.quote(key.user_id) + ", "
                     + txn.quote(key.name) + ", "
                     + txn.quote(key.created_at) + ")");

            txn.exec("INSERT INTO s3_api_keys (api_key_id, access_key, secret_access_key, region, endpoint) VALUES ("
                     + txn.quote(key.id) + ", "
                     + txn.quote(key.access_key) + ", "
                     + txn.quote(key.secret_access_key) + ", "
                     + txn.quote(key.region) + ", "
                     + txn.quote(key.endpoint) + ")");

            txn.commit();
        });
    }

    void APIKeyQueries::removeAPIKey(unsigned int keyId) {
        Transactions::exec("APIKeyQueries::removeAPIKey", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM s3_api_keys WHERE id = " + txn.quote(keyId));
            txn.commit();
        });
    }

    std::vector<vh::types::S3APIKey> APIKeyQueries::listS3APIKeys(unsigned int userId) {
        return Transactions::exec("APIKeyQueries::listS3APIKeys", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM s3_api_keys WHERE user_id = " + txn.quote(userId));
            std::vector<vh::types::S3APIKey> keys;
            for (const auto& row : res) keys.emplace_back(row);
            return keys;
        });
    }

    vh::types::S3APIKey APIKeyQueries::getS3APIKey(unsigned int keyId) {
        return Transactions::exec("APIKeyQueries::getS3APIKey", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM s3_api_keys WHERE id = " + txn.quote(keyId));
            if (res.empty()) throw std::runtime_error("No S3 API key found with ID: " + std::to_string(keyId));
            return vh::types::S3APIKey(res[0]);
        });
    }
}

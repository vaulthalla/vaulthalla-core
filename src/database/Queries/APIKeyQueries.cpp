#include "database/Queries/APIKeyQueries.hpp"
#include "database/Transactions.hpp"
#include "types/APIKey.hpp"

using namespace vh::database;
using namespace vh::types::api;

unsigned int APIKeyQueries::upsertAPIKey(const std::shared_ptr<APIKey>& key) {
    return Transactions::exec("APIKeyQueries::addAPIKey", [&](pqxx::work& txn) {
        pqxx::binarystring enc_secret(key->encrypted_secret_access_key.data(), key->encrypted_secret_access_key.size());
        pqxx::binarystring iv(key->iv.data(), key->iv.size());
        pqxx::params p{
            key->user_id,
            key->name,
            to_string(key->provider),
            key->access_key,
            enc_secret,
            iv,
            key->region,
            key->endpoint
        };
        return txn.exec_prepared("upsert_api_key", p).one_field().as<unsigned int>();
    });
}

void APIKeyQueries::removeAPIKey(const unsigned int keyId) {
    Transactions::exec("APIKeyQueries::removeAPIKey", [&](pqxx::work& txn) {
        txn.exec_prepared("remove_api_key", keyId);
    });
}

std::vector<std::shared_ptr<APIKey> > APIKeyQueries::listAPIKeys(const unsigned int userId) {
    return Transactions::exec("APIKeyQueries::listAPIKeys", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("list_user_api_keys", userId);
        return api_keys_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<APIKey> > APIKeyQueries::listAPIKeys() {
    return Transactions::exec("APIKeyQueries::listAPIKeys", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("list_api_keys");
        return api_keys_from_pq_res(res);
    });
}

std::shared_ptr<APIKey> APIKeyQueries::getAPIKey(const unsigned int keyId) {
    return Transactions::exec("APIKeyQueries::getAPIKey", [&](pqxx::work& txn) -> std::shared_ptr<APIKey> {
        const auto res = txn.exec_prepared("get_api_key", keyId);
        if (res.empty()) return nullptr;
        return std::make_shared<APIKey>(res.one_row());
    });
}

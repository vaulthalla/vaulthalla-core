#include "database/queries/APIKeyQueries.hpp"
#include "database/Transactions.hpp"
#include "vault/model/APIKey.hpp"
#include "database/encoding/bytea.hpp"

using namespace vh::database;
using namespace vh::database::model;
using namespace vh::vault::model;
using namespace vh::database::encoding;

unsigned int APIKeyQueries::upsertAPIKey(const std::shared_ptr<APIKey>& key) {
    return Transactions::exec("APIKeyQueries::addAPIKey", [&](pqxx::work& txn) {
        pqxx::params p{
            key->user_id,
            key->name,
            to_string(key->provider),
            key->access_key,
            to_hex_bytea(key->encrypted_secret_access_key),
            to_hex_bytea(key->iv),
            key->region,
            key->endpoint
        };
        return txn.exec(pqxx::prepped{"upsert_api_key"}, p).one_field().as<unsigned int>();
    });
}

void APIKeyQueries::removeAPIKey(const unsigned int keyId) {
    Transactions::exec("APIKeyQueries::removeAPIKey", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"remove_api_key"}, keyId);
    });
}

std::vector<std::shared_ptr<APIKey> > APIKeyQueries::listAPIKeys(const unsigned int userId, const ListQueryParams& params) {
    return Transactions::exec("APIKeyQueries::listAPIKeys", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT * FROM api_keys WHERE user_id = " + txn.quote(userId),
            params, "id", "name"
        );
        return api_keys_from_pq_res(txn.exec(sql));
    });
}

std::vector<std::shared_ptr<APIKey> > APIKeyQueries::listAPIKeys(const ListQueryParams& params) {
    return Transactions::exec("APIKeyQueries::listAPIKeys", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT * FROM api_keys",
            params, "id", "name"
        );
        return api_keys_from_pq_res(txn.exec(sql));
    });
}

std::shared_ptr<APIKey> APIKeyQueries::getAPIKey(const unsigned int keyId) {
    return Transactions::exec("APIKeyQueries::getAPIKey", [&](pqxx::work& txn) -> std::shared_ptr<APIKey> {
        const auto res = txn.exec(pqxx::prepped{"get_api_key"}, keyId);
        if (res.empty()) return nullptr;
        return std::make_shared<APIKey>(res.one_row());
    });
}

std::shared_ptr<APIKey> APIKeyQueries::getAPIKey(const std::string& keyName) {
    if (keyName.empty()) return nullptr;

    return Transactions::exec("APIKeyQueries::getAPIKeyByName", [&](pqxx::work& txn) -> std::shared_ptr<APIKey> {
        const auto res = txn.exec(pqxx::prepped{"get_api_key_by_name"}, keyName);
        if (res.empty()) return nullptr;
        return std::make_shared<APIKey>(res.one_row());
    });
}

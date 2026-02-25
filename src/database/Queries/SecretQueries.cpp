#include "database/queries/InternalSecretQueries.hpp"
#include "database/Transactions.hpp"
#include "crypto/model/Secret.hpp"
#include "database/encoding/bytea.hpp"

using namespace vh::database;
using namespace vh::crypto::model;
using namespace vh::database::encoding;

void SecretQueries::upsertSecret(const std::shared_ptr<Secret>& secret) {
    Transactions::exec("SecretQueries::upsertSecret", [&](pqxx::work& txn) {
        pqxx::params p{secret->key, to_hex_bytea(secret->value), to_hex_bytea(secret->iv)};
        txn.exec(pqxx::prepped{"upsert_internal_secret"}, p);
    });
}

std::shared_ptr<Secret> SecretQueries::getSecret(const std::string& key) {
    return Transactions::exec("SecretQueries::getSecret", [&](pqxx::work& txn) -> std::shared_ptr<Secret> {
        const auto res = txn.exec(pqxx::prepped{"get_internal_secret"}, key);
        if (res.empty()) return nullptr;
        return std::make_shared<Secret>(res.one_row());
    });
}

bool SecretQueries::secretExists(const std::string& key) {
    return Transactions::exec("SecretQueries::secretExists", [&](pqxx::work& txn) -> bool {
        const auto res = txn.exec(pqxx::prepped{"internal_secret_exists"}, key);
        if (res.empty()) return false;
        return res.one_field().as<bool>();
    });
}

#include "database/Queries/InternalSecretQueries.hpp"
#include "database/Transactions.hpp"
#include "types/InternalSecret.hpp"

using namespace vh::database;
using namespace vh::types;

void InternalSecretQueries::upsertSecret(const std::shared_ptr<types::InternalSecret>& secret) {
    Transactions::exec("InternalSecretQueries::upsertSecret", [&](pqxx::work& txn) {
        pqxx::binarystring value_blob(secret->value.data(), secret->value.size());
        pqxx::binarystring iv_blob(secret->iv.data(), secret->iv.size());
        pqxx::params p{secret->key, value_blob, iv_blob};
        txn.exec(pqxx::prepped{"upsert_internal_secret"}, p);
    });
}

std::shared_ptr<InternalSecret> InternalSecretQueries::getSecret(const std::string& key) {
    return Transactions::exec("InternalSecretQueries::getSecret", [&](pqxx::work& txn) -> std::shared_ptr<types::InternalSecret> {
        const auto res = txn.exec(pqxx::prepped{"get_internal_secret"}, key);
        if (res.empty()) return nullptr;
        return std::make_shared<types::InternalSecret>(res.one_row());
    });
}

bool InternalSecretQueries::secretExists(const std::string& key) {
    return Transactions::exec("InternalSecretQueries::secretExists", [&](pqxx::work& txn) -> bool {
        const auto res = txn.exec(pqxx::prepped{"internal_secret_exists"}, key);
        if (res.empty()) return false;
        return res.one_field().as<bool>();
    });
}

#include "database/Queries/VaultKeyQueries.hpp"
#include "database/Transactions.hpp"
#include "types/VaultKey.hpp"

using namespace vh::database;
using namespace vh::types;

void VaultKeyQueries::addVaultKey(const std::shared_ptr<VaultKey>& key) {
    Transactions::exec("VaultKeyQueries::addVaultKey", [&](pqxx::work& txn) {
        pqxx::binarystring enc_key(key->encrypted_key.data(), key->encrypted_key.size());
        pqxx::binarystring iv(key->iv.data(), key->iv.size());
        pqxx::params p{key->vaultId, enc_key, iv};
        txn.exec_prepared("insert_vault_key", p);
    });
}

void VaultKeyQueries::deleteVaultKey(unsigned int vaultId) {
    Transactions::exec("VaultKeyQueries::deleteVaultKey", [&](pqxx::work& txn) {
        txn.exec_prepared("delete_vault_key", vaultId);
    });
}

void VaultKeyQueries::updateVaultKey(const std::shared_ptr<VaultKey>& key) {
    Transactions::exec("VaultKeyQueries::updateVaultKey", [&](pqxx::work& txn) {
        pqxx::binarystring enc_key(key->encrypted_key.data(), key->encrypted_key.size());
        pqxx::binarystring iv(key->iv.data(), key->iv.size());
        pqxx::params p{key->vaultId, enc_key, iv};
        txn.exec_prepared("update_vault_key", p);
    });
}

std::shared_ptr<VaultKey> VaultKeyQueries::getVaultKey(unsigned int vaultId) {
    return Transactions::exec("VaultKeyQueries::getVaultKey", [&](pqxx::work& txn) -> std::shared_ptr<VaultKey> {
        auto res = txn.exec_prepared("get_vault_key", vaultId);
        if (res.empty()) return nullptr;
        return std::make_shared<VaultKey>(res[0]);
    });
}

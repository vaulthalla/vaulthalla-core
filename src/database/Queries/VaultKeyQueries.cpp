#include "database/Queries/VaultKeyQueries.hpp"
#include "database/Transactions.hpp"
#include "types/VaultKey.hpp"

using namespace vh::database;
using namespace vh::types;

unsigned int VaultKeyQueries::addVaultKey(const std::shared_ptr<VaultKey>& key) {
    return Transactions::exec("VaultKeyQueries::addVaultKey", [&](pqxx::work& txn) {
        pqxx::binarystring enc_key(key->encrypted_key.data(), key->encrypted_key.size());
        pqxx::binarystring iv(key->iv.data(), key->iv.size());
        pqxx::params p{key->vaultId, enc_key, iv};
        const auto res = txn.exec(pqxx::prepped{"insert_vault_key"}, p);
        if (res.empty()) throw std::runtime_error("Failed to add vault key: no result returned");
        return res.one_field().as<unsigned int>();
    });
}

void VaultKeyQueries::deleteVaultKey(unsigned int vaultId) {
    Transactions::exec("VaultKeyQueries::deleteVaultKey", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"delete_vault_key"}, vaultId);
    });
}

void VaultKeyQueries::updateVaultKey(const std::shared_ptr<VaultKey>& key) {
    Transactions::exec("VaultKeyQueries::updateVaultKey", [&](pqxx::work& txn) {
        pqxx::binarystring enc_key(key->encrypted_key.data(), key->encrypted_key.size());
        pqxx::binarystring iv(key->iv.data(), key->iv.size());
        pqxx::params p{key->vaultId, enc_key, iv};
        txn.exec(pqxx::prepped{"update_vault_key"}, p);
    });
}

std::shared_ptr<VaultKey> VaultKeyQueries::getVaultKey(unsigned int vaultId) {
    return Transactions::exec("VaultKeyQueries::getVaultKey", [&](pqxx::work& txn) -> std::shared_ptr<VaultKey> {
        const auto res = txn.exec(pqxx::prepped{"get_vault_key"}, vaultId);
        if (res.empty()) return nullptr;
        return std::make_shared<VaultKey>(res[0]);
    });
}

unsigned int VaultKeyQueries::rotateVaultKey(const std::shared_ptr<VaultKey>& newKey) {
    return Transactions::exec("VaultKeyQueries::rotateVaultKey", [&](pqxx::work& txn) {
        pqxx::binarystring enc_key(newKey->encrypted_key.data(), newKey->encrypted_key.size());
        pqxx::binarystring iv(newKey->iv.data(), newKey->iv.size());
        pqxx::params p{newKey->vaultId, enc_key, iv};
        const auto res = txn.exec(pqxx::prepped{"rotate_vault_key"}, p);
        if (res.empty()) throw std::runtime_error("Failed to rotate vault key: no result returned");
        return res.one_field().as<unsigned int>();
    });
}

void VaultKeyQueries::markKeyRotationFinished(unsigned int vaultId) {
    Transactions::exec("VaultKeyQueries::markKeyRotationFinished", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"mark_vault_key_rotation_finished"}, vaultId);
    });
}

bool VaultKeyQueries::keyRotationInProgress(unsigned int vaultId) {
    return Transactions::exec("VaultKeyQueries::keyRotationInProgress", [&](pqxx::work& txn) {
        return txn.exec(pqxx::prepped{"vault_key_rotation_in_progress"}, vaultId).one_field().as<bool>();
    });
}

std::shared_ptr<VaultKey> VaultKeyQueries::getRotationInProgressOldKey(unsigned int vaultId) {
    return Transactions::exec("VaultKeyQueries::getRotationInProgressOldKey", [&](pqxx::work& txn) -> std::shared_ptr<VaultKey> {
        const auto res = txn.exec(pqxx::prepped{"get_rotation_old_vault_key"}, vaultId);
        if (res.empty()) throw std::runtime_error("No old vault key found for rotation in progress");
        return std::make_shared<VaultKey>(res[0]);
    });
}

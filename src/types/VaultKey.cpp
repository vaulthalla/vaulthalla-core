#include "types/VaultKey.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types;

VaultKey::VaultKey(const pqxx::row &row)
    : vaultId(row["vault_id"].as<unsigned int>()),
      version(row["version"].as<unsigned int>()),
      created_at(parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(parsePostgresTimestamp(row["updated_at"].as<std::string>())) {

    if (!row["encrypted_key"].is_null()) {
        const pqxx::binarystring enc_key(row["encrypted_key"]);
        encrypted_key.assign(enc_key.begin(), enc_key.end());
    }

    if (!row["iv"].is_null()) {
        const pqxx::binarystring i(row["iv"]);
        iv.assign(i.begin(), i.end());
    }
}

void to_json(nlohmann::json &j, const VaultKey &vk) {
    j = nlohmann::json{
        {"vault_id", vk.vaultId},
        {"key", vk.key},
        {"created_at", timestampToString(vk.created_at)},
        {"updated_at", timestampToString(vk.updated_at)}
    };
}

void from_json(const nlohmann::json &j, VaultKey &vk) {
    j.at("vault_id").get_to(vk.vaultId);
    j.at("key").get_to(vk.key);
}

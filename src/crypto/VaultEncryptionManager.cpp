#include "crypto/VaultEncryptionManager.hpp"
#include "crypto/encrypt.hpp"
#include "services/LogRegistry.hpp"
#include "database/Queries/VaultKeyQueries.hpp"
#include "types/VaultKey.hpp"

#include <sodium.h>
#include <stdexcept>

using namespace vh::crypto;
using namespace vh::logging;
using namespace vh::database;

VaultEncryptionManager::VaultEncryptionManager(const unsigned int vault_id)
    : vault_id_(vault_id) {
    tpmKeyProvider_ = std::make_unique<TPMKeyProvider>();
    tpmKeyProvider_->init();
    load_key();
}

void VaultEncryptionManager::load_key() {
    const auto rec = VaultKeyQueries::getVaultKey(vault_id_);
    const auto masterKey = tpmKeyProvider_->getMasterKey();

    if (!rec) {
        // First time: generate and seal new vault key
        std::vector<uint8_t> vaultKey(AES_KEY_SIZE);
        randombytes_buf(vaultKey.data(), AES_KEY_SIZE);

        std::vector<uint8_t> iv;
        const auto enc_key = encrypt_aes256_gcm(vaultKey, masterKey, iv);

        const auto key = std::make_shared<types::VaultKey>();
        key->vaultId = vault_id_;
        key->encrypted_key = enc_key;
        key->iv = iv;
        VaultKeyQueries::addVaultKey(key);

        key_ = std::move(vaultKey);
        LogRegistry::crypto()->info("[VaultEncryptionManager] Created new sealed key for vault {}", vault_id_);
        return;
    }

    // Decrypt existing vault key
    key_ = decrypt_aes256_gcm(rec->encrypted_key, masterKey, rec->iv);
    if (key_.size() != AES_KEY_SIZE)
        throw std::runtime_error("Vault key must be 32 bytes (AES-256)");
}

std::vector<uint8_t> VaultEncryptionManager::encrypt(const std::vector<uint8_t>& plaintext, std::string& out_b64_iv) const {
    std::vector<uint8_t> iv;
    auto ciphertext = encrypt_aes256_gcm(plaintext, key_, iv);
    out_b64_iv = b64_encode(iv);
    return ciphertext;
}

std::vector<uint8_t> VaultEncryptionManager::decrypt(const std::vector<uint8_t>& ciphertext, const std::string& b64_iv) const {
    return decrypt_aes256_gcm(ciphertext, key_, b64_decode(b64_iv));
}

std::vector<uint8_t> VaultEncryptionManager::get_key(const std::string& callingFunctionName) const {
    LogRegistry::audit()->info("[VaultEncryptionManager] Accessing vault key for vault ID: {} in function: {}",
                                               vault_id_, callingFunctionName);
    return key_;
}


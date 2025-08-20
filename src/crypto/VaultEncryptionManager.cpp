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

unsigned int VaultEncryptionManager::get_key_version() const { return version_; }


void VaultEncryptionManager::load_key() {
    rotation_in_progress_.store(VaultKeyQueries::keyRotationInProgress(vault_id_));
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
        key->version = VaultKeyQueries::addVaultKey(key);
        version_ = key->version;

        key_ = std::move(vaultKey);
        LogRegistry::crypto()->info("[VaultEncryptionManager] Created new sealed key for vault {}", vault_id_);
        return;
    }

    version_ = rec->version;

    // Decrypt existing vault key
    key_ = decrypt_aes256_gcm(rec->encrypted_key, masterKey, rec->iv);
    if (key_.size() != AES_KEY_SIZE)
        throw std::runtime_error("Vault key must be 32 bytes (AES-256)");

    if (rotation_in_progress_) {
        new_key_ = key_;
        key_.clear();

        const auto oldKey = VaultKeyQueries::getRotationInProgressOldKey(vault_id_);
        if (!oldKey) {
            LogRegistry::crypto()->error("[VaultEncryptionManager] No old key found for rotation in progress for vault {}",
                                         vault_id_);
            throw std::runtime_error("No old key found for rotation in progress");
        }

        key_ = decrypt_aes256_gcm(oldKey->encrypted_key, masterKey, oldKey->iv);
        if (key_.size() != AES_KEY_SIZE) {
            LogRegistry::crypto()->error("[VaultEncryptionManager] Old vault key must be 32 bytes (AES-256), got {} bytes",
                                         key_.size());
            throw std::runtime_error("Old vault key must be 32 bytes (AES-256)");
        }

        LogRegistry::crypto()->info("[VaultEncryptionManager] Loaded old key for vault {} during rotation", vault_id_);
    }
}

void VaultEncryptionManager::prepare_key_rotation() {
    if (VaultKeyQueries::keyRotationInProgress(vault_id_)) {
        LogRegistry::crypto()->warn("[VaultEncryptionManager] Key rotation already in progress for vault {}", vault_id_);
        return;
    }

    LogRegistry::crypto()->info("[VaultEncryptionManager] Preparing key rotation for vault {}", vault_id_);
    new_key_.clear();
    new_key_.resize(AES_KEY_SIZE);
    randombytes_buf(new_key_.data(), AES_KEY_SIZE);

    std::vector<uint8_t> iv;
    const auto key = std::make_shared<types::VaultKey>();
    key->vaultId = vault_id_;
    key->encrypted_key = encrypt_aes256_gcm(new_key_, tpmKeyProvider_->getMasterKey(), iv);
    key->iv = std::move(iv);
    key->version = VaultKeyQueries::rotateVaultKey(key);
    version_ = key->version;

    LogRegistry::crypto()->info("[VaultEncryptionManager] Prepared new key for vault {}", vault_id_);
}

void VaultEncryptionManager::finish_key_rotation() {
    if (!VaultKeyQueries::keyRotationInProgress(vault_id_)) {
        LogRegistry::crypto()->warn("[VaultEncryptionManager] No key rotation in progress for vault {}", vault_id_);
        return;
    }

    key_.clear();
    key_ = std::move(new_key_);
    new_key_.clear();

    VaultKeyQueries::markKeyRotationFinished(vault_id_);
    rotation_in_progress_.store(false);

    LogRegistry::crypto()->info("[VaultEncryptionManager] Finished key rotation for vault {}", vault_id_);
}

std::vector<uint8_t> VaultEncryptionManager::rotateDecryptEncrypt(const std::vector<uint8_t>& ciphertext, std::string& b64_iv_ref) const {
    const auto decrypted = decrypt_aes256_gcm(ciphertext, key_, b64_decode(b64_iv_ref));
    std::vector<uint8_t> iv;
    const auto encrypted = encrypt_aes256_gcm(decrypted, new_key_, iv);
    if (encrypted.size() != ciphertext.size()) {
        LogRegistry::crypto()->error("[VaultEncryptionManager] Encrypted data size mismatch after key rotation");
        throw std::runtime_error("Encrypted data size mismatch after key rotation");
    }
    b64_iv_ref = b64_encode(iv);
    LogRegistry::crypto()->info("[VaultEncryptionManager] Successfully rotated key for vault {}", vault_id_);
    return encrypted;
}

std::vector<uint8_t> VaultEncryptionManager::encrypt(const std::vector<uint8_t>& plaintext, std::string& out_b64_iv, const unsigned int keyVersion) const {
    std::vector<uint8_t> iv;

    auto ciphertext = encrypt_aes256_gcm(plaintext, key_, iv);
    out_b64_iv = b64_encode(iv);
    return ciphertext;
}

std::vector<uint8_t> VaultEncryptionManager::decrypt(const std::vector<uint8_t>& ciphertext, const std::string& b64_iv, const unsigned int keyVersion) const {
    if (rotation_in_progress_.load()) {
        if (key_.empty() || new_key_.empty()) throw std::runtime_error("Key rotation in progress but keys are not set");

        if (keyVersion == version_) return decrypt_aes256_gcm(ciphertext, new_key_, b64_decode(b64_iv));
        if (keyVersion == version_ - 1) return decrypt_aes256_gcm(ciphertext, key_, b64_decode(b64_iv));

        if (keyVersion < version_ - 1)
            LogRegistry::crypto()->warn("[VaultEncryptionManager] Key version {} is too old for vault {}, using new key",
                                        keyVersion, vault_id_);
        else if (keyVersion > version_)
            LogRegistry::crypto()->warn("[VaultEncryptionManager] Key version {} is newer than current version {} for vault {}, using new key",
                                        keyVersion, version_, vault_id_);

        return decrypt_aes256_gcm(ciphertext, new_key_, b64_decode(b64_iv));
    }

    if (keyVersion != version_) {
        LogRegistry::crypto()->warn("[VaultEncryptionManager] Key version mismatch: expected {}, got {} for vault {}",
                                    version_, keyVersion, vault_id_);
        throw std::runtime_error("Key version mismatch");
    }

    return decrypt_aes256_gcm(ciphertext, key_, b64_decode(b64_iv));
}

std::vector<uint8_t> VaultEncryptionManager::get_key(const std::string& callingFunctionName) const {
    LogRegistry::audit()->info("[VaultEncryptionManager] Accessing vault key for vault ID: {} in function: {}",
                                               vault_id_, callingFunctionName);
    return key_;
}


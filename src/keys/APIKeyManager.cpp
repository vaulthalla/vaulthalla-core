#include "keys/APIKeyManager.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "crypto/encrypt.hpp"
#include "types/APIKey.hpp"

#include <sodium.h>
#include <stdexcept>

using namespace vh::keys;
using namespace vh::types::api;

APIKeyManager::APIKeyManager() {
    // Initialize TPM master key provider
    tpmKeyProvider_ = std::make_unique<crypto::TPMKeyProvider>();
    tpmKeyProvider_->init();

    initAPIKeys();
}

void APIKeyManager::initAPIKeys() {
    std::lock_guard lock(apiKeysMutex_);
    auto keys = database::APIKeyQueries::listAPIKeys();
    for (const auto& key : keys) {
        apiKeys_[key->id] = key;
    }
}

void APIKeyManager::addAPIKey(std::shared_ptr<APIKey>& key) {
    std::lock_guard lock(apiKeysMutex_);

    // --- Encrypt secret_access_key before storage ---
    std::vector<uint8_t> masterKey = tpmKeyProvider_->getMasterKey();
    std::vector<uint8_t> iv;
    const auto plaintext = std::vector<uint8_t>(key->secret_access_key.begin(),
                                          key->secret_access_key.end());

    const auto ciphertext = crypto::encrypt_aes256_gcm(plaintext, masterKey, iv);

    // Replace sensitive data with encrypted representation
    key->encrypted_secret_access_key = ciphertext;
    key->iv = iv;
    key->secret_access_key.clear(); // wipe plaintext from memory

    // Persist to DB
    key->id = database::APIKeyQueries::addAPIKey(key);

    // Refresh from DB (ensures created_at, etc. are up to date)
    key = database::APIKeyQueries::getAPIKey(key->id);

    // Cache in memory
    apiKeys_[key->id] = key;
}

void APIKeyManager::removeAPIKey(unsigned int keyId, unsigned int userId) {
    std::lock_guard lock(apiKeysMutex_);

    auto it = apiKeys_.find(keyId);
    if (it != apiKeys_.end()) {
        if (it->second->user_id != userId) {
            throw std::runtime_error("API key does not belong to the user");
        }
        apiKeys_.erase(it);
    } else {
        auto key = database::APIKeyQueries::getAPIKey(keyId);
        if (!key) throw std::runtime_error("API key not found");
        if (key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
    }

    database::APIKeyQueries::removeAPIKey(keyId);
}

std::vector<std::shared_ptr<APIKey>> APIKeyManager::listAPIKeys() const {
    std::lock_guard lock(apiKeysMutex_);
    return database::APIKeyQueries::listAPIKeys();
}

std::vector<std::shared_ptr<APIKey>> APIKeyManager::listUserAPIKeys(unsigned int userId) const {
    std::lock_guard lock(apiKeysMutex_);
    return database::APIKeyQueries::listAPIKeys(userId);
}

std::shared_ptr<APIKey> APIKeyManager::getAPIKey(unsigned int keyId, unsigned int userId) const {
    std::lock_guard lock(apiKeysMutex_);

    auto key = database::APIKeyQueries::getAPIKey(keyId);
    if (!key) throw std::runtime_error("API key not found");
    if (key->user_id != userId) throw std::runtime_error("API key does not belong to the user");

    // --- Decrypt secret_access_key before returning ---
    std::vector<uint8_t> masterKey = tpmKeyProvider_->getMasterKey();
    auto decrypted = crypto::decrypt_aes256_gcm(
        key->encrypted_secret_access_key, masterKey, key->iv);

    key->secret_access_key.assign(decrypted.begin(), decrypted.end());

    return key;
}

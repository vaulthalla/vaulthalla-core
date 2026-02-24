#include "vault/APIKeyManager.hpp"
#include "database/Queries/APIKeyQueries.hpp"
#include "crypto/encrypt.hpp"
#include "vault/model/APIKey.hpp"

#include <sodium.h>
#include <stdexcept>
#include <paths.h>

using namespace vh::crypto;
using namespace vh::vault;
using namespace vh::vault::model;
using namespace vh::database;

APIKeyManager::APIKeyManager() {
    const std::string tpmKeyName = paths::testMode ? "test_ak_master" : "ak_master";
    tpmKeyProvider_ = std::make_unique<TPMKeyProvider>(tpmKeyName);
    tpmKeyProvider_->init();

    initAPIKeys();
}

void APIKeyManager::initAPIKeys() {
    std::scoped_lock lock(apiKeysMutex_);
    auto keys = APIKeyQueries::listAPIKeys();
    for (const auto& key : keys) {
        apiKeys_[key->id] = key;
    }
}

unsigned int APIKeyManager::addAPIKey(std::shared_ptr<APIKey>& key) {
    std::scoped_lock lock(apiKeysMutex_);

    // --- Encrypt secret_access_key before storage ---
    const auto masterKey = tpmKeyProvider_->getMasterKey();
    std::vector<uint8_t> iv;
    const auto plaintext = std::vector<uint8_t>(key->secret_access_key.begin(),
                                          key->secret_access_key.end());

    const auto ciphertext = encrypt_aes256_gcm(plaintext, masterKey, iv);

    // Replace sensitive data with encrypted representation
    key->encrypted_secret_access_key = ciphertext;
    key->iv = iv;
    key->secret_access_key.clear(); // wipe plaintext from memory

    // Persist to DB
    key->id = APIKeyQueries::upsertAPIKey(key);

    // Refresh from DB (ensures created_at, etc. are up to date)
    key = APIKeyQueries::getAPIKey(key->id);

    // Cache in memory
    apiKeys_[key->id] = key;

    return key->id;
}

void APIKeyManager::removeAPIKey(const unsigned int keyId, const unsigned int userId) {
    std::scoped_lock lock(apiKeysMutex_);

    const auto it = apiKeys_.find(keyId);
    if (it != apiKeys_.end()) {
        if (it->second->user_id != userId) {
            throw std::runtime_error("API key does not belong to the user");
        }
        apiKeys_.erase(it);
    } else {
        const auto key = APIKeyQueries::getAPIKey(keyId);
        if (!key) throw std::runtime_error("API key not found");
        if (key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
    }

    APIKeyQueries::removeAPIKey(keyId);
}

std::vector<std::shared_ptr<APIKey>> APIKeyManager::listAPIKeys() const {
    std::scoped_lock lock(apiKeysMutex_);
    return APIKeyQueries::listAPIKeys();
}

std::vector<std::shared_ptr<APIKey>> APIKeyManager::listUserAPIKeys(unsigned int userId) const {
    std::scoped_lock lock(apiKeysMutex_);
    return APIKeyQueries::listAPIKeys(userId);
}

std::shared_ptr<APIKey> APIKeyManager::getAPIKey(unsigned int keyId, unsigned int userId) const {
    std::scoped_lock lock(apiKeysMutex_);

    auto key = APIKeyQueries::getAPIKey(keyId);
    if (!key) throw std::runtime_error("API key not found");
    if (key->user_id != userId) throw std::runtime_error("API key does not belong to the user");

    // --- Decrypt secret_access_key before returning ---
    const auto masterKey = tpmKeyProvider_->getMasterKey();
    auto decrypted = decrypt_aes256_gcm(
        key->encrypted_secret_access_key, masterKey, key->iv);

    key->secret_access_key.assign(decrypted.begin(), decrypted.end());

    return key;
}

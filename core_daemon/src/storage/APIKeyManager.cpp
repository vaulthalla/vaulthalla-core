#include "storage/APIKeyManager.hpp"
#include "database/Queries/APIKeyQueries.hpp"

namespace vh::keys {
APIKeyManager::APIKeyManager() {
    initAPIKeys();
}

void APIKeyManager::initAPIKeys() {
    std::lock_guard lock(apiKeysMutex_);
    auto keys = database::APIKeyQueries::listAPIKeys();
    for (const auto& key : keys) apiKeys_[key->id] = key;
}

void APIKeyManager::addAPIKey(std::shared_ptr<types::api::APIKey>& key) {
    std::lock_guard lock(apiKeysMutex_);
    // TODO: generate master key on install and use it to encrypt API secrets
    key->id = database::APIKeyQueries::addAPIKey(key);
    key = database::APIKeyQueries::getAPIKey(key->id);
    apiKeys_[key->id] = key; // Store in local cache
}

void APIKeyManager::removeAPIKey(unsigned short keyId, unsigned short userId) {
    std::lock_guard lock(apiKeysMutex_);
    if (apiKeys_.find(keyId) != apiKeys_.end()) {
        auto key = apiKeys_.at(keyId);
        if (key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
        else
            apiKeys_.erase(keyId);
    } else {
        auto key = database::APIKeyQueries::getAPIKey(keyId);
        if (key && key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
        else if (!key)
            throw std::runtime_error("API key not found");
    }
    database::APIKeyQueries::removeAPIKey(keyId);
}

std::vector<std::shared_ptr<types::api::APIKey>> APIKeyManager::listUserAPIKeys(unsigned short userId) const {
    std::lock_guard lock(apiKeysMutex_);
    return database::APIKeyQueries::listAPIKeys(userId);
}

std::vector<std::shared_ptr<types::api::APIKey>> APIKeyManager::listAPIKeys() const {
    std::lock_guard lock(apiKeysMutex_);
    return database::APIKeyQueries::listAPIKeys();
}

std::shared_ptr<types::api::APIKey> APIKeyManager::getAPIKey(unsigned short keyId, unsigned short userId) const {
    std::lock_guard lock(apiKeysMutex_);
    if (apiKeys_.find(keyId) != apiKeys_.end()) {
        auto key = apiKeys_.at(keyId);
        if (key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
    }
    return database::APIKeyQueries::getAPIKey(keyId);
}
} // namespace vh::keys

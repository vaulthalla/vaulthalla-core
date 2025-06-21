#include "keys/APIKeyManager.hpp"
#include "database/Queries/APIKeyQueries.hpp"

namespace vh::keys {
APIKeyManager::APIKeyManager() {
    initAPIKeys();
}

void APIKeyManager::initAPIKeys() {
    std::lock_guard<std::mutex> lock(apiKeysMutex_);
    auto keys = vh::database::APIKeyQueries::listAPIKeys();
    for (const auto& key : keys) apiKeys_[key->id] = key;
}

void APIKeyManager::addAPIKey(std::shared_ptr<vh::types::api::APIKey>& key) {
    std::lock_guard<std::mutex> lock(apiKeysMutex_);
    key->id = vh::database::APIKeyQueries::addAPIKey(key);
    key = vh::database::APIKeyQueries::getAPIKey(key->id);
    apiKeys_[key->id] = key; // Store in local cache
}

void APIKeyManager::removeAPIKey(unsigned short keyId, unsigned short userId) {
    std::lock_guard<std::mutex> lock(apiKeysMutex_);
    if (apiKeys_.find(keyId) != apiKeys_.end()) {
        auto key = apiKeys_.at(keyId);
        if (key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
        else
            apiKeys_.erase(keyId);
    } else {
        auto key = vh::database::APIKeyQueries::getAPIKey(keyId);
        if (key && key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
        else if (!key)
            throw std::runtime_error("API key not found");
    }
    vh::database::APIKeyQueries::removeAPIKey(keyId);
}

std::vector<std::shared_ptr<vh::types::api::APIKey>> APIKeyManager::listUserAPIKeys(unsigned short userId) const {
    std::lock_guard<std::mutex> lock(apiKeysMutex_);
    return vh::database::APIKeyQueries::listAPIKeys(userId);
}

std::vector<std::shared_ptr<vh::types::api::APIKey>> APIKeyManager::listAPIKeys() const {
    std::lock_guard<std::mutex> lock(apiKeysMutex_);
    return vh::database::APIKeyQueries::listAPIKeys();
}

std::shared_ptr<vh::types::api::APIKey> APIKeyManager::getAPIKey(unsigned short keyId, unsigned short userId) const {
    std::lock_guard<std::mutex> lock(apiKeysMutex_);
    if (apiKeys_.find(keyId) != apiKeys_.end()) {
        auto key = apiKeys_.at(keyId);
        if (key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
    }
    return vh::database::APIKeyQueries::getAPIKey(keyId);
}
} // namespace vh::keys

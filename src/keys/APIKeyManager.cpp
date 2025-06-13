#include "keys/APIKeyManager.hpp"
#include "database/Queries/APIKeyQueries.hpp"

namespace vh::keys {
    APIKeyManager::APIKeyManager() { initAPIKeys(); }

    void APIKeyManager::initAPIKeys() {
        auto keys = vh::database::APIKeyQueries::listAPIKeys();
        for (const auto& key : keys) apiKeys_[key->id] = key;
    }

    void APIKeyManager::addAPIKey(std::shared_ptr<vh::types::APIKey>& key) {
        vh::database::APIKeyQueries::addAPIKey(key);
        key = vh::database::APIKeyQueries::getAPIKey(key->id);
        apiKeys_[key->id] = key; // Store in local cache
    }

    void APIKeyManager::removeAPIKey(unsigned short keyId, unsigned short userId) {
        if (apiKeys_.find(keyId) != apiKeys_.end()) {
            auto key = apiKeys_.at(keyId);
            if (key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
            else apiKeys_.erase(keyId);
        } else {
            auto key = vh::database::APIKeyQueries::getAPIKey(keyId);
            if (key && key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
            else if (!key) throw std::runtime_error("API key not found");
        }
        vh::database::APIKeyQueries::removeAPIKey(keyId);
    }

    std::vector<std::shared_ptr<vh::types::APIKey>> APIKeyManager::listAPIKeys(unsigned short userId) const {
        return vh::database::APIKeyQueries::listAPIKeys(userId);
    }

    std::shared_ptr<vh::types::APIKey> APIKeyManager::getAPIKey(unsigned short keyId, unsigned short userId) const {
        if (apiKeys_.find(keyId) != apiKeys_.end()) {
            auto key = apiKeys_.at(keyId);
            if (key->user_id != userId) throw std::runtime_error("API key does not belong to the user");
        }
        return vh::database::APIKeyQueries::getAPIKey(keyId);
    }
}

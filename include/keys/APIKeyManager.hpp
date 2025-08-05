#pragma once

#include "types/APIKey.hpp"
#include "keys/TPMKeyProvider.hpp"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace vh::keys {

class APIKeyManager {
public:
    APIKeyManager();

    void initAPIKeys();

    unsigned int addAPIKey(std::shared_ptr<types::api::APIKey>& key);
    void removeAPIKey(unsigned int keyId, unsigned int userId);

    [[nodiscard]] std::vector<std::shared_ptr<types::api::APIKey>> listAPIKeys() const;
    [[nodiscard]] std::vector<std::shared_ptr<types::api::APIKey>> listUserAPIKeys(unsigned int userId) const;
    [[nodiscard]] std::shared_ptr<types::api::APIKey> getAPIKey(unsigned int keyId, unsigned int userId) const;

private:
    mutable std::mutex apiKeysMutex_;
    std::unordered_map<unsigned int, std::shared_ptr<types::api::APIKey>> apiKeys_;

    std::unique_ptr<crypto::TPMKeyProvider> tpmKeyProvider_;
};

}

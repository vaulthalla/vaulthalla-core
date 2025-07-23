#pragma once

#include "../../../shared/include/types/APIKey.hpp"
#include <memory>
#include <mutex>

namespace vh::keys {

class APIKeyManager {
  public:
    APIKeyManager();

    void initAPIKeys();

    void addAPIKey(std::shared_ptr<types::api::APIKey>& key);
    void removeAPIKey(unsigned short keyId, unsigned short userId);
    [[nodiscard]] std::vector<std::shared_ptr<types::api::APIKey>> listAPIKeys() const;
    [[nodiscard]] std::vector<std::shared_ptr<types::api::APIKey>> listUserAPIKeys(unsigned short userId) const;
    [[nodiscard]] std::shared_ptr<types::api::APIKey> getAPIKey(unsigned short keyId, unsigned short userId) const;

  private:
    mutable std::mutex apiKeysMutex_;
    std::unordered_map<unsigned int, std::shared_ptr<types::api::APIKey>> apiKeys_{};
};

} // namespace vh::keys

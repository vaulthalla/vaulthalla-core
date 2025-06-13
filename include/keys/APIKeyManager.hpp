#pragma once

#include <memory>
#include "types/APIKey.hpp"

namespace vh::keys {

    class APIKeyManager {
    public:
        APIKeyManager();

        void initAPIKeys();

        void addAPIKey(std::shared_ptr<vh::types::APIKey>& key);
        void removeAPIKey(unsigned short keyId, unsigned short userId);
        [[nodiscard]] std::vector<std::shared_ptr<vh::types::APIKey>> listAPIKeys(unsigned short userId) const;
        [[nodiscard]] std::shared_ptr<vh::types::APIKey> getAPIKey(unsigned short keyId, unsigned short userId) const;

    private:
        std::unordered_map<unsigned short, std::shared_ptr<vh::types::APIKey>> apiKeys_{};
    };

}

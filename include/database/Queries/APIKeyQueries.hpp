#pragma once

#include "types/APIKey.hpp"
#include <memory>

namespace vh::database {

    struct APIKeyQueries {
        APIKeyQueries() = default;

        static void addAPIKey(const std::shared_ptr<vh::types::APIKey>& key);

        static void removeAPIKey(unsigned int keyId);

        static std::vector<std::shared_ptr<vh::types::APIKey>> listAPIKeys(unsigned int userId);

        static std::vector<std::shared_ptr<vh::types::APIKey>> listAPIKeys();

        static std::shared_ptr<vh::types::APIKey> getAPIKey(unsigned int keyId);
    };

}

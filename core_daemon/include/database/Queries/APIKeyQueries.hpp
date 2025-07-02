#pragma once

#include "../../../../shared/include/types/APIKey.hpp"
#include <memory>

namespace vh::database {

struct APIKeyQueries {
    APIKeyQueries() = default;

    static unsigned int addAPIKey(const std::shared_ptr<types::api::APIKey>& key);

    static void removeAPIKey(unsigned int keyId);

    static std::vector<std::shared_ptr<types::api::APIKey>> listAPIKeys(unsigned int userId);

    static std::vector<std::shared_ptr<types::api::APIKey>> listAPIKeys();

    static std::shared_ptr<types::api::APIKey> getAPIKey(unsigned int keyId);
};

} // namespace vh::database

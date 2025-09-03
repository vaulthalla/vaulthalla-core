#pragma once

#include "types/APIKey.hpp"
#include "types/ListQueryParams.hpp"

#include <memory>

namespace vh::database {

struct APIKeyQueries {
    APIKeyQueries() = default;

    static unsigned int upsertAPIKey(const std::shared_ptr<types::api::APIKey>& key);

    static void removeAPIKey(unsigned int keyId);

    static std::vector<std::shared_ptr<types::api::APIKey>> listAPIKeys(unsigned int userId, const types::ListQueryParams& params = {});

    static std::vector<std::shared_ptr<types::api::APIKey>> listAPIKeys(const types::ListQueryParams& params = {});

    static std::shared_ptr<types::api::APIKey> getAPIKey(unsigned int keyId);

    static std::shared_ptr<types::api::APIKey> getAPIKey(const std::string& keyName);
};

} // namespace vh::database

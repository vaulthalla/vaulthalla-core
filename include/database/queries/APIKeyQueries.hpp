#pragma once

#include "database/model/ListQueryParams.hpp"

#include <memory>
#include <vector>

namespace vh::vault::model { struct APIKey; }

namespace vh::database {

struct APIKeyQueries {
    APIKeyQueries() = default;

    static unsigned int upsertAPIKey(const std::shared_ptr<vault::model::APIKey>& key);

    static void removeAPIKey(unsigned int keyId);

    static std::vector<std::shared_ptr<vault::model::APIKey>> listAPIKeys(unsigned int userId, const model::ListQueryParams& params = {});

    static std::vector<std::shared_ptr<vault::model::APIKey>> listAPIKeys(const model::ListQueryParams& params = {});

    static std::shared_ptr<vault::model::APIKey> getAPIKey(unsigned int keyId);

    static std::shared_ptr<vault::model::APIKey> getAPIKey(const std::string& keyName);
};

}

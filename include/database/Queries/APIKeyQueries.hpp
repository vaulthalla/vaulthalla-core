#pragma once

#include "types/APIKey.hpp"

namespace vh::database {

    struct APIKeyQueries {
        APIKeyQueries() = default;

        static void addS3APIKey(const vh::types::S3APIKey &key);

        static void removeAPIKey(unsigned int keyId);

        static std::vector<vh::types::S3APIKey> listS3APIKeys(unsigned int userId);

        static vh::types::S3APIKey getS3APIKey(unsigned int keyId);
    };

}

#pragma once

#include "types/StorageBackend.hpp"

namespace vh::database {

    struct StorageBackendQueries {
        StorageBackendQueries() = default;

        static void addStorageBackend(const vh::types::StorageBackend& backend);
        static void removeStorageBackend(unsigned int backendId);
        static std::vector<vh::types::StorageBackend> listStorageBackends();
        static vh::types::StorageBackend getStorageBackend(unsigned int backendId);
    };

}

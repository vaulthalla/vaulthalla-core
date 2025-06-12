#pragma once

#include "types/StorageVolume.hpp"
#include "types/UserStorageVolume.hpp"

namespace vh::database {

    struct StorageQueries {
        StorageQueries() = default;

        static void addStorageVolume(const vh::types::StorageVolume& volume,
                                     const vh::types::UserStorageVolume& userVolume);
        static void removeStorageVolume(unsigned int volumeId);
        static std::vector<vh::types::StorageVolume> listStorageVolumes(unsigned int userId);
        static vh::types::StorageVolume getStorageVolume(unsigned int volumeId);
    };

}

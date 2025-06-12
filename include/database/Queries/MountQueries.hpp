#pragma once

#include "types/StorageBackend.hpp"

#include <string>
#include <unordered_map>
#include <variant>

namespace vh::database {

    using mount_variant = std::variant<
            vh::types::LocalDiskConfig,
            vh::types::S3Config
    >;

    using mount_pair = std::pair<
            vh::types::StorageBackend,
            mount_variant
    >;

    class MountQueries {
    public:
        MountQueries() = default;

        static void addLocalMount(const vh::types::LocalDiskConfig& config);

        static void addS3Mount(const vh::types::StorageBackend& backend,
                               const vh::types::S3Config& config);

        static void removeMount(const std::string& mountName);

        static bool mountExists(const std::string& mountName);

        static std::unordered_map<std::string, mount_pair> listMounts();
    };

}

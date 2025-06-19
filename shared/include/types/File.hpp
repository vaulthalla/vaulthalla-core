#pragma once

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <optional>

namespace vh::types {

    struct File {
        unsigned int id;
        unsigned int storage_volume_id;
        std::optional<unsigned int> parent_id;
        std::string name;
        bool is_directory = false;
        unsigned long long mode;
        unsigned int owner_id;
        unsigned int created_by;
        std::time_t created_at;
        std::time_t updated_at;
        unsigned long long current_version_size_bytes;
        bool is_trashed;
        std::time_t trashed_at;
        unsigned int trashed_by;
        std::optional<std::string> full_path;
    };

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::File, (),
                      (id, storage_volume_id, parent_id, name, is_directory, mode, owner_id,
                       created_by, created_at, updated_at, current_version_size_bytes,
                       is_trashed, trashed_at, trashed_by, full_path))

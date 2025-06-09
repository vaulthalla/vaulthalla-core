#pragma once

#include <string>
#include <boost/describe.hpp>
#include <ctime>
#include <optional>

namespace vh::types {

    struct Group {
        unsigned int id;
        std::string name;
        std::optional<std::string> description;
        std::time_t created_at;
        std::optional<std::time_t> updated_at;
    };

    struct GroupMembership {
        unsigned int group_id;
        unsigned int user_id;
        std::time_t joined_at;
    };

    struct GroupStorageVolume {
        unsigned int group_id;
        unsigned int storage_volume_id;
        std::time_t assigned_at;
    };

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::Group, (), (id, name, description, created_at, updated_at))

BOOST_DESCRIBE_STRUCT(vh::types::GroupMembership, (), (group_id, user_id, joined_at))

BOOST_DESCRIBE_STRUCT(vh::types::GroupStorageVolume, (), (group_id, storage_volume_id, assigned_at))

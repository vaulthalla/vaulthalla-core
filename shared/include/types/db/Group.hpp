#pragma once

#include <boost/describe.hpp>
#include <ctime>
#include <optional>
#include <string>

namespace vh::types {

struct Group {
    unsigned int id;
    std::string name;
    std::optional<std::string> description;
    std::time_t created_at;
    std::optional<std::time_t> updated_at;
};

struct GroupMember {
    unsigned int gid;
    unsigned int uid;
    std::time_t joined_at;
};

struct GroupStorageVolume {
    unsigned int gid;
    unsigned int storage_volume_id;
    std::time_t assigned_at;
};

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::Group, (), (id, name, description, created_at, updated_at))

BOOST_DESCRIBE_STRUCT(vh::types::GroupMember, (), (gid, uid, joined_at))

BOOST_DESCRIBE_STRUCT(vh::types::GroupStorageVolume, (), (gid, storage_volume_id, assigned_at))

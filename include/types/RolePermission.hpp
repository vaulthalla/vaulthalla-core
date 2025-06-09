#pragma once

#include <string>
#include <boost/describe.hpp>
#include <optional>

namespace vh::types {
    struct RolePermission {
        unsigned int role_id;
        unsigned int permission_id;
        std::optional<unsigned int> storage_volume_id;
    };
}

BOOST_DESCRIBE_STRUCT(vh::types::RolePermission, (),
                      (role_id, permission_id, storage_volume_id))

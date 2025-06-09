#pragma once

#include <string>
#include <boost/describe.hpp>

namespace vh::types {

    struct UserStorageVolume {
        unsigned int user_id;
        unsigned int storage_volume_id;
    };

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::UserStorageVolume, (),
                      (user_id, storage_volume_id))

#pragma once

#include <string>
#include <boost/describe.hpp>
#include <ctime>

namespace vh::types {
    struct FileActivity {
        unsigned int id;
        unsigned int file_id;
        unsigned int user_id;
        std::string action; // e.g., "created", "updated", "deleted"
        std::time_t timestamp;
    };
}

BOOST_DESCRIBE_STRUCT(vh::types::FileActivity, (), (id, file_id, user_id, action, timestamp))

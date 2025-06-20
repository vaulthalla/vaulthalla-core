#pragma once

#include <boost/describe.hpp>
#include <ctime>
#include <string>

namespace vh::types {
struct FileActivity {
    unsigned int id;
    unsigned int file_id;
    unsigned int user_id;
    std::string action; // e.g., "created", "updated", "deleted"
    std::time_t timestamp;
};
} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::FileActivity, (), (id, file_id, user_id, action, timestamp))

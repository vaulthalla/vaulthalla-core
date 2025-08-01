#pragma once

#include <boost/describe.hpp>
#include <ctime>
#include <string>

namespace vh::types {

struct FileTag {
    unsigned int id;
    std::string name;
};

struct FileTagAssignment {
    unsigned int file_id;
    unsigned int tag_id;
    std::time_t assigned_at;
};

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::FileTag, (), (id, name))

BOOST_DESCRIBE_STRUCT(vh::types::FileTagAssignment, (), (file_id, tag_id, assigned_at))

#pragma once

#include <boost/describe.hpp>
#include <ctime>
#include <optional>

namespace vh::types {

struct FileLock {
    unsigned int file_id;
    unsigned int locked_by;
    std::time_t locked_at;
    std::optional<std::time_t> expires_at;
};

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::FileLock, (), (file_id, locked_by, locked_at, expires_at))

#pragma once

#include <boost/describe.hpp>
#include <ctime>
#include <string>

namespace vh::types {

struct AuditLog {
    unsigned int id;
    unsigned int user_id;
    std::string action;
    unsigned int target_file_id;
    std::time_t timestamp;
    std::string ip_address;
    std::string user_agent;
};

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::AuditLog, (), (id, user_id, action, target_file_id, timestamp, ip_address, user_agent))

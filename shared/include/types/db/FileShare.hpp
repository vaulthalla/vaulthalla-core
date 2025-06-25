#pragma once

#include <ctime>
#include <optional>
#include <string>
#include <pqxx/row>

namespace vh::types {

struct FileShare {
    unsigned int id;
    unsigned int file_acl_id;
    unsigned int shared_by;
    std::string share_token;
    std::optional<std::time_t> expires_at;
    std::time_t created_at;

    explicit FileShare(const pqxx::row& row);
};

} // namespace vh::types

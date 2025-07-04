#pragma once

#include "types/Permission.hpp"

#include <string>
#include <ctime>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct UserRole {
    unsigned int id;
    std::string name, description;
    std::time_t created_at;
    uint16_t permissions{0};

    UserRole() = default;
    explicit UserRole(const pqxx::row& row);
    explicit UserRole(const nlohmann::json& j);
};

void to_json(nlohmann::json& j, const UserRole& r);
void from_json(const nlohmann::json& j, UserRole& r);

std::vector<std::shared_ptr<UserRole>> userRolesFromPqRes(const pqxx::result& res);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<UserRole>>& roles);

} // namespace vh::types

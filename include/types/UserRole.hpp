#pragma once

#include "types/Permission.hpp"
#include "types/Role.hpp"

#include <string>
#include <ctime>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct UserRole final : Role {
    unsigned int assignment_id, user_id;
    std::time_t assigned_at;

    UserRole() = default;
    explicit UserRole(const pqxx::row& row);
    explicit UserRole(const nlohmann::json& j);
};

void to_json(nlohmann::json& j, const UserRole& r);
void from_json(const nlohmann::json& j, UserRole& r);

std::vector<std::shared_ptr<UserRole>> user_roles_from_pq_res(const pqxx::result& res);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<UserRole>>& roles);

std::string to_string(const std::shared_ptr<UserRole>& role);

} // namespace vh::types

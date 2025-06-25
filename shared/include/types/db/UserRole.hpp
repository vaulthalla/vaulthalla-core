#pragma once

#include "types/db/Role.hpp"

#include <string>
#include <ctime>
#include <nlohmann/json_fwd.hpp>
#include <optional>

namespace pqxx {
    class row;
}

namespace vh::types {

struct UserRole : Role {
    unsigned int id, user_id, role_id;
    std::string scope;
    std::optional<unsigned int> scope_id; // Optional for global roles
    std::time_t assigned_at;

    explicit UserRole(const pqxx::row& row);
    explicit UserRole(const nlohmann::json& row);
};

void to_json(nlohmann::json& j, const UserRole& ur);
void from_json(const nlohmann::json& j, UserRole& ur);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<UserRole>>& user_roles);
std::vector<std::shared_ptr<UserRole>> user_roles_from_json(const nlohmann::json& j);

}

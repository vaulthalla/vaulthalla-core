#pragma once

#include "types/db/BaseRole.hpp"

#include <string>
#include <ctime>
#include <nlohmann/json_fwd.hpp>
#include <optional>

namespace pqxx {
    class row;
}

namespace vh::types {

struct Role : BaseRole {
    unsigned int id, role_id, subject_id;
    std::string scope;
    std::optional<unsigned int> scope_id; // Optional for global roles
    std::time_t assigned_at;
    bool inherited;

    explicit Role(const pqxx::row& row);
    explicit Role(const nlohmann::json& row);
};

void to_json(nlohmann::json& j, const Role& ur);
void from_json(const nlohmann::json& j, Role& ur);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Role>>& user_roles);
std::vector<std::shared_ptr<Role>> user_roles_from_json(const nlohmann::json& j);

}

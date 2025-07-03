#pragma once

#include "Role.hpp"

#include <string>
#include <ctime>
#include <optional>
#include <memory>
#include <vector>
#include <nlohmann/json_fwd.hpp>
#include <pqxx/result.hxx>

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct AssignedRole : Role {
    unsigned int id, subject_id, role_id, vault_id;
    std::string subject_type;   // 'user' or 'group'
    std::time_t assigned_at;

    AssignedRole() = default;
    explicit AssignedRole(const pqxx::row& row);
    explicit AssignedRole(const nlohmann::json& j);
};

// JSON + DB helpers
void to_json(nlohmann::json& j, const AssignedRole& r);
void from_json(const nlohmann::json& j, AssignedRole& r);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<AssignedRole>>& roles);
std::vector<std::shared_ptr<AssignedRole>> assigned_roles_from_json(const nlohmann::json& j);
std::vector<std::shared_ptr<AssignedRole>> assigned_roles_from_pq_result(const pqxx::result& res);

} // namespace vh::types

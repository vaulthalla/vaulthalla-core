#include "types/UserRole.hpp"
#include "util/timestamp.hpp"
#include "util/cmdLineHelpers.hpp"

#include <pqxx/result>
#include <nlohmann/json.hpp>

#include "usages.hpp"

#include <sstream>

using namespace vh::types;

UserRole::UserRole(const pqxx::row& row)
    : Role(row) {
    assignment_id = row["assignment_id"].as<unsigned int>();
    user_id = row["user_id"].as<unsigned int>();
    assigned_at = util::parsePostgresTimestamp(row["assigned_at"].as<std::string>());
}

UserRole::UserRole(const nlohmann::json& j)
    : Role(j) {
    assignment_id = j.at("assignment_id").get<unsigned int>();
    user_id = j.at("user_id").get<unsigned int>();
    assigned_at = util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>());
}

std::string UserRole::permissions_to_flags_string() const {
    std::ostringstream oss;
    for (unsigned int i = 0; i < ADMIN_SHELL_PERMS.size(); ++i) {
        const auto bit = 1 << i;
        if (oss.tellp() > 0) oss << ' ';
        if (permissions & bit) oss << "--allow-" << ADMIN_SHELL_PERMS[i];
        else oss << "--deny-" << ADMIN_SHELL_PERMS[i];
    }
    return oss.str();
}

void vh::types::to_json(nlohmann::json& j, const UserRole& r) {
    to_json(j, static_cast<const Role&>(r));
    j["assignment_id"] = r.assignment_id;
    j["user_id"] = r.user_id;
    j["assigned_at"] = util::timestampToString(r.assigned_at);
}

void vh::types::from_json(const nlohmann::json& j, UserRole& r) {
    from_json(j, static_cast<Role&>(r));
    r.assignment_id = j.at("assignment_id").get<unsigned int>();
    r.user_id = j.at("user_id").get<unsigned int>();
    r.assigned_at = util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>());
}

std::vector<std::shared_ptr<UserRole>> vh::types::user_roles_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<UserRole>> roles;
    roles.reserve(res.size());
    for (const auto& row : res) roles.push_back(std::make_shared<UserRole>(row));
    return roles;
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<UserRole>>& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
}

std::string vh::types::to_string(const std::shared_ptr<UserRole>& role) {
    const std::string prefix(4, ' ');
    std::string out = shell::snake_case_to_title(role->name) + " (ID: " + std::to_string(role->id) + ")\n";
    out += prefix + "- Role ID: " + std::to_string(role->id) + "\n";
    out += prefix + "- Description: " + role->description + "\n";
    out += prefix + "- Type: " + role->type + "\n";
    out += prefix + "- Created at: " + util::timestampToString(role->created_at) + "\n";
    out += prefix + "- Assigned at: " + util::timestampToString(role->assigned_at) + "\n";
    out += prefix + "- Permissions:\n" + admin_perms_to_string(role->permissions, 12);
    return out;
}

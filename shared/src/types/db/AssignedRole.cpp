#include "types/db/AssignedRole.hpp"
#include "util/timestamp.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types;

AssignedRole::AssignedRole(const pqxx::row& row)
    : Role(row),
      id(row["id"].as<unsigned int>()),
      subject_type(row["subject_type"].as<std::string>()),
      subject_id(row["subject_id"].as<unsigned int>()),
      role_id(row["role_id"].as<unsigned int>()),
      scope(row["scope"].as<std::string>()),
      assigned_at(util::parsePostgresTimestamp(row["assigned_at"].as<std::string>())),
      inherited(row["inherited"].as<bool>()) {
    if (!row["scope_id"].is_null()) scope_id = row["scope_id"].as<unsigned int>();
}

AssignedRole::AssignedRole(const nlohmann::json& j)
    : Role(j),
      id(j.at("id").get<unsigned int>()),
      subject_type(j.at("subject_type").get<std::string>()),
      subject_id(j.at("subject_id").get<unsigned int>()),
      role_id(j.at("role_id").get<unsigned int>()),
      scope(j.at("scope").get<std::string>()),
      assigned_at(util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>())),
      inherited(j.value("inherited", false))
{
    if (j.contains("scope_id") && !j["scope_id"].is_null()) scope_id = j["scope_id"].get<unsigned int>();
}

void vh::types::to_json(nlohmann::json& j, const AssignedRole& r) {
    to_json(j, static_cast<const Role&>(r));  // fill base Role part first
    j.update({
        {"id", r.id},
        {"subject_type", r.subject_type},
        {"subject_id", r.subject_id},
        {"role_id", r.role_id},
        {"scope", r.scope},
        {"assigned_at", util::timestampToString(r.assigned_at)},
        {"inherited", r.inherited}
    });
    if (r.scope_id.has_value()) j["scope_id"] = r.scope_id.value();
}

void vh::types::from_json(const nlohmann::json& j, AssignedRole& r) {
    from_json(j, static_cast<Role&>(r));
    r.id = j.at("id").get<unsigned int>();
    r.subject_type = j.at("subject_type").get<std::string>();
    r.subject_id = j.at("subject_id").get<unsigned int>();
    r.role_id = j.at("role_id").get<unsigned int>();
    r.scope = j.at("scope").get<std::string>();
    r.assigned_at = util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>());
    r.inherited = j.value("inherited", false);

    if (j.contains("scope_id") && !j["scope_id"].is_null()) r.scope_id = j["scope_id"].get<unsigned int>();
    else r.scope_id.reset();
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<AssignedRole>>& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
}

std::vector<std::shared_ptr<AssignedRole>> vh::types::roles_from_json(const nlohmann::json& j) {
    std::vector<std::shared_ptr<AssignedRole>> roles;
    for (const auto& roleJson : j) roles.push_back(std::make_shared<AssignedRole>(roleJson));
    return roles;
}

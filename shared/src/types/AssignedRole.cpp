#include "types/AssignedRole.hpp"
#include "util/timestamp.hpp"

#include <pqxx/row>
#include <pqxx/result>
#include <nlohmann/json.hpp>

using namespace vh::types;

AssignedRole::AssignedRole(const pqxx::row& row)
    : Role(row),
      id(row["id"].as<unsigned int>()),
subject_id(row["subject_id"].as<unsigned int>()),
role_id(row["role_id"].as<unsigned int>()),
      vault_id(row["vault_id"].as<unsigned int>()),
      subject_type(row["subject_type"].as<std::string>()),
      assigned_at(util::parsePostgresTimestamp(row["assigned_at"].as<std::string>())) {}

AssignedRole::AssignedRole(const nlohmann::json& j)
    : Role(j),
      id(j.at("id").get<unsigned int>()),
subject_id(j.at("subject_id").get<unsigned int>()),
role_id(j.at("role_id").get<unsigned int>()),
vault_id(j.at("vault_id").get<unsigned int>()),
      subject_type(j.at("subject_type").get<std::string>()),
      assigned_at(util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>())) {
}

void vh::types::to_json(nlohmann::json& j, const AssignedRole& r) {
    to_json(j, static_cast<const Role&>(r)); // fill base Role part first
    j.update({
        {"id", r.id},
        {"vault_id", r.vault_id},
        {"subject_type", r.subject_type},
        {"subject_id", r.subject_id},
        {"role_id", r.role_id},
        {"assigned_at", util::timestampToString(r.assigned_at)}
    });
}

void vh::types::from_json(const nlohmann::json& j, AssignedRole& r) {
    from_json(j, static_cast<Role&>(r));
    r.id = j.at("id").get<unsigned int>();
    r.vault_id = j.at("vault_id").get<unsigned int>();
    r.subject_type = j.at("subject_type").get<std::string>();
    r.subject_id = j.at("subject_id").get<unsigned int>();
    r.role_id = j.at("role_id").get<unsigned int>();
    r.assigned_at = util::parsePostgresTimestamp(j.at("assigned_at").get<std::string>());
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<AssignedRole> >& roles) {
    j = nlohmann::json::array();
    for (const auto& role : roles) j.push_back(*role);
}

std::vector<std::shared_ptr<AssignedRole> > vh::types::assigned_roles_from_json(const nlohmann::json& j) {
    std::vector<std::shared_ptr<AssignedRole> > roles;
    for (const auto& roleJson : j) roles.push_back(std::make_shared<AssignedRole>(roleJson));
    return roles;
}

std::vector<std::shared_ptr<AssignedRole>> vh::types::assigned_roles_from_pq_result(const pqxx::result& res) {
    std::vector<std::shared_ptr<AssignedRole> > roles;
    for (const auto& item : res) roles.push_back(std::make_shared<AssignedRole>(item));
    return roles;
}
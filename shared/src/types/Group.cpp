#include "../../include/types/Group.hpp"
#include "util/timestamp.hpp"
#include "../../include/types/User.hpp"

#include <pqxx/row>
#include <pqxx/result>
#include <nlohmann/json.hpp>

using namespace vh::types;

GroupMember::GroupMember(const pqxx::row& row)
    : user(std::make_shared<User>(row)),
      joined_at(util::parsePostgresTimestamp(row["joined_at"].as<std::string>())) {}

Group::Group(const pqxx::row& gr, const pqxx::result& members, const pqxx::result& storageVolumes)
                 : id(gr["id"].as<unsigned int>()),
                   name(gr["name"].as<std::string>()),
                   description(gr["description"].is_null() ? std::nullopt : std::make_optional(gr["description"].as<std::string>())),
                   created_at(util::parsePostgresTimestamp(gr["created_at"].as<std::string>())),
                   updated_at(gr["updated_at"].is_null() ? std::nullopt : std::make_optional(util::parsePostgresTimestamp(gr["updated_at"].as<std::string>()))),
                   members() {
    for (const auto& memberRow : members) this->members.push_back(std::make_shared<GroupMember>(memberRow));
}

Group::Group(const nlohmann::json& j)
    : id(j.at("id").get<unsigned int>()),
      name(j.at("name").get<std::string>()),
      description(j.contains("description") ? std::make_optional(j.at("description").get<std::string>()) : std::nullopt),
      created_at(util::parsePostgresTimestamp(j.at("created_at").get<std::string>())),
      updated_at(j.contains("updated_at") && !j["updated_at"].is_null()
                     ? std::make_optional(util::parsePostgresTimestamp(j.at("updated_at").get<std::string>()))
                     : std::nullopt) {
    for (const auto& memberJson : j.at("members")) {
        auto member = std::make_shared<GroupMember>();
        member->user = std::make_shared<User>();
        member->user->id = memberJson.at("user_id").get<unsigned int>();
        member->joined_at = util::parsePostgresTimestamp(memberJson.at("joined_at").get<std::string>());
        members.push_back(member);
    }
}

void vh::types::to_json(nlohmann::json& j, const Group& g) {
    nlohmann::json members = nlohmann::json::array();
    for (const auto& member : g.members) members.push_back(*member);

    j = {
        {"id", g.id},
        {"name", g.name},
        {"description", g.description},
        {"created_at", util::timestampToString(g.created_at)},
        {"members", members}
    };

    if (g.updated_at.has_value()) j["updated_at"] = util::timestampToString(*g.updated_at);
}

void vh::types::from_json(const nlohmann::json& j, Group& g) {
    g.id = j.at("id").get<unsigned int>();
    g.name = j.at("name").get<std::string>();
    g.description = j.contains("description") ? std::make_optional(j.at("description").get<std::string>()) : std::nullopt;
    g.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
    g.updated_at = j.contains("updated_at") && !j["updated_at"].is_null()
                       ? std::make_optional(util::parsePostgresTimestamp(j.at("updated_at").get<std::string>()))
                       : std::nullopt;

    for (const auto& memberJson : j.at("members")) {
        auto member = std::make_shared<GroupMember>();
        member->user = std::make_shared<User>();
        member->user->id = memberJson.at("user_id").get<unsigned int>();
        member->joined_at = util::parsePostgresTimestamp(memberJson.at("joined_at").get<std::string>());
        g.members.push_back(member);
    }
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Group>>& groups) {
    j = nlohmann::json::array();
    for (const auto& group : groups) j.push_back(*group);
}

std::vector<std::shared_ptr<Group>> vh::types::groups_from_json(const nlohmann::json& j) {
    std::vector<std::shared_ptr<Group>> groups;
    for (const auto& groupJson : j) groups.push_back(std::make_shared<Group>(groupJson));
    return groups;
}

void vh::types::to_json(nlohmann::json& j, const GroupMember& gm) {
    j = {
        {"user", *gm.user},
        {"joined_at", util::timestampToString(gm.joined_at)}
    };
}

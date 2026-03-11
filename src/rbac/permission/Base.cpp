#include "rbac/permission/Permission.hpp"

#include "db/encoding/timestamp.hpp"
#include <nlohmann/json.hpp>

using namespace vh::db::encoding;

namespace vh::rbac::permission {

Permission::Permission(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      description(row["description"].as<std::string>()),
      bit_position(row["bit_position"].as<uint16_t>()),
      created_at(parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(parsePostgresTimestamp(row["updated_at"].as<std::string>())) {
}

Permission::Permission(const nlohmann::json& j)
    : id(j.at("id").get<unsigned int>()),
      name(j.at("name").get<std::string>()),
      description(j.at("description").get<std::string>()),
      bit_position(j.at("bit_position").get<uint16_t>()),
      created_at(parsePostgresTimestamp(j.at("created_at").get<std::string>())),
      updated_at(parsePostgresTimestamp(j.at("updated_at").get<std::string>())) {
}

Permission::Permission(const unsigned int bitPos, std::string name, std::string description)
    : name(std::move(name)),
      description(std::move(description)),
      bit_position(bitPos) {}

void to_json(nlohmann::json& j, const Permission& p) {
    j = {
        {"id", p.id},
        {"name", p.name},

        {"description", p.description},
        {"bit_position", p.bit_position},
        {"created_at", timestampToString(p.created_at)},
        {"updated_at", timestampToString(p.updated_at)}
    };
}

void from_json(const nlohmann::json& j, Permission& p) {
    p.id = j.at("id").get<unsigned int>();
    p.name = j.at("name").get<std::string>();
    p.description = j.at("description").get<std::string>();
    p.bit_position = j.at("bit_position").get<uint16_t>();
}

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Permission> >& permissions) {
    j = nlohmann::json::array();
    for (const auto& perm : permissions) j.push_back(*perm);
}

}

#include "rbac/role/Meta.hpp"
#include "db/encoding/timestamp.hpp"
#include "db/encoding/has.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>
#include <ostream>

using namespace vh::db::encoding;

namespace vh::rbac::role {
    BasicMeta::BasicMeta(const pqxx::row& row) {
        if (const auto v = try_get<std::string>(row, std::vector<std::string_view>{"role_created_at", "created_at"}))
            created_at = parsePostgresTimestamp(*v);

        if (const auto v = try_get<std::string>(row, std::vector<std::string_view>{"role_updated_at", "updated_at"}))
            updated_at = parsePostgresTimestamp(*v);

        if (const auto v = try_get<std::string>(row, std::vector<std::string_view>{"role_assigned_at", "assigned_at"}))
            assigned_at = parsePostgresTimestamp(*v);
    }

    Meta::Meta(const pqxx::row& row)
        : BasicMeta(row) {
        if (const auto v = try_get<uint32_t>(row, std::vector<std::string_view>{"role_id", "id"}))
            id = *v;

        if (const auto v = try_get<uint32_t>(row, std::vector<std::string_view>{"role_assignment_id", "assignment_id"}))
            assignment_id = *v;

        if (const auto v = try_get<std::string>(row, std::vector<std::string_view>{"role_name", "name"}))
            name = *v;

        if (const auto v = try_get<std::string>(row, std::vector<std::string_view>{"role_description", "description"}))
            description = *v;
    }

    Meta::Meta(const nlohmann::json &json) { from_json(json, *this); }

    std::string BasicMeta::toString(const uint8_t indent) const {
        const std::string in(indent + 2, ' ');
        std::ostringstream oss;
        oss << in << "- Created At: " << timestampToString(created_at) << std::endl;
        oss << in << "- Updated At: " << timestampToString(updated_at) << std::endl;
        if (assigned_at) oss << in << "- Assigned At: " << timestampToString(*assigned_at) << std::endl;
        return oss.str();
    }

    std::string Meta::toString(const uint8_t indent) const {
        const std::string in(indent + 2, ' ');
        std::ostringstream oss;
        oss << in << "- Id: " << std::to_string(id) << std::endl;
        oss << in << "- Assignment ID: " << std::to_string(assignment_id) << std::endl;
        oss << in << "- Name: " << name << std::endl;
        oss << in << "- Description: " << description << std::endl;
        oss << BasicMeta::toString(indent);
        return oss.str();
    }

    void to_json(nlohmann::json &j, const BasicMeta &m) {
        j = {
            {"created_at", m.created_at},
            {"updated_at", m.updated_at},
            {"assigned_at", m.assigned_at},
        };
    }

    void to_json(nlohmann::json &j, const Meta &m) {
        j = static_cast<BasicMeta>(m);
        j["id"] = m.id;
        j["assigned_at"] = m.assigned_at;
        j["name"] = m.name;
        j["description"] = m.description;
    }

    void from_json(const nlohmann::json &j, Meta &m) {
        // keep this tight, don't let client dictate timestamps or id's
        j.at("name").get_to(m.name);
        j.at("description").get_to(m.description);
    }
}

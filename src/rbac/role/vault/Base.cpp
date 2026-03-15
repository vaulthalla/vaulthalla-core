#include "../../../../include/rbac/role/vault/Base.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/result>
#include <ostream>

namespace vh::rbac::permission {

Vault::Vault(const pqxx::row& row)
    : roles(row["roles_permissions"].as<typename decltype(roles)::Mask>()),
      sync(row["sync_permissions"].as<typename decltype(sync)::Mask>()),
      filesystem(row) {}

Vault::Vault(const pqxx::row& row, const pqxx::result& overrides)
    : roles(row["roles_permissions"].as<typename decltype(roles)::Mask>()),
      sync(row["sync_permissions"].as<typename decltype(sync)::Mask>()),
      filesystem(row, overrides) {}

std::string Vault::toFlagsString() const {
    std::ostringstream oss;
    oss << roles.toFlagsString()
        << sync.toFlagsString()
        << filesystem.toFlagString();
    return oss.str();
}

std::string Vault::toString(const uint8_t indent) const {
    std::ostringstream oss;
    oss << std::string(indent, ' ') << "Vault Permissions:\n";
    const auto i = indent + 2;
    oss << roles.toString(i);
    oss << sync.toString(i);
    oss << filesystem.toString(i);
    return oss.str();
}

void to_json(nlohmann::json& j, const Vault& v) {
    j = {
        {"roles", v.roles},
        {"sync", v.sync},
        {"filesystem", v.filesystem},
    };
}

void from_json(const nlohmann::json& j, Vault& v) {
    j.at("roles").get_to(v.roles);
    j.at("sync").get_to(v.sync);
    j.at("filesystem").get_to(v.filesystem);
}

}

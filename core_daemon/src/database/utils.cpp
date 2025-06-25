#include "database/utils.hpp"
#include <sstream>

namespace vh::database {

std::string buildRoleValuesList(
    const std::set<std::tuple<int, std::string, std::optional<int>>>& roles
) {
    std::ostringstream oss;
    bool first = true;

    for (const auto& [role_id, scope, scoped_id] : roles) {
        if (!first) oss << ", ";
        first = false;

        oss << "("
            << role_id << ", "
            << "'" << scope << "', "
            << (scoped_id.has_value() ? std::to_string(*scoped_id) : "NULL")
            << ")";
    }

    return oss.str();
}

} // namespace vh::database

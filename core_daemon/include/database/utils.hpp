#pragma once

#include <set>
#include <tuple>
#include <string>
#include <optional>

namespace vh::database {

std::string buildRoleValuesList(
    const std::set<std::tuple<int, std::string, std::optional<int>>>& roles
);

}

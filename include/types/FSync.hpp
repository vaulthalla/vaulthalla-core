#pragma once

#include "types/Sync.hpp"

#include <string>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
}

namespace vh::types {

struct FSync : public Sync {
    enum class ConflictPolicy { Overwrite, KeepBoth, Ask };

    ConflictPolicy conflict_policy{ConflictPolicy::KeepBoth};

    FSync() = default;
    explicit FSync(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const FSync& s);
void from_json(const nlohmann::json& j, FSync& s);

std::string to_string(const FSync::ConflictPolicy& cp);

FSync::ConflictPolicy fsConflictPolicyFromString(const std::string& str);

std::string to_string(const std::shared_ptr<FSync>& sync);

}

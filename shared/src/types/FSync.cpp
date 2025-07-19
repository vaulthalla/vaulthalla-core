#include "types/FSync.hpp"

#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types;

FSync::FSync(const pqxx::row& row)
    : Sync(row),
      conflict_policy(fsConflictPolicyFromString(row.at("conflict_policy").as<std::string>())) {}

void vh::types::to_json(nlohmann::json& j, const FSync& s) {
    to_json(j, static_cast<const Sync&>(s));
    j["conflict_policy"] = to_string(s.conflict_policy);
}

void vh::types::from_json(const nlohmann::json& j, FSync& s) {
    from_json(j, static_cast<Sync&>(s));
    s.conflict_policy = fsConflictPolicyFromString(j.at("conflict_policy").get<std::string>());
}

std::string vh::types::to_string(const FSync::ConflictPolicy& cp) {
    switch (cp) {
        case FSync::ConflictPolicy::Overwrite: return "overwrite";
        case FSync::ConflictPolicy::KeepBoth: return "keep_both";
        case FSync::ConflictPolicy::Ask: return "ask";
        default: throw std::invalid_argument("Unknown conflict policy");
    }
}

FSync::ConflictPolicy vh::types::fsConflictPolicyFromString(const std::string& str) {
    if (str == "overwrite") return FSync::ConflictPolicy::Overwrite;
    if (str == "keep_both") return FSync::ConflictPolicy::KeepBoth;
    if (str == "ask") return FSync::ConflictPolicy::Ask;
    throw std::invalid_argument("Unknown conflict policy: " + str);
}

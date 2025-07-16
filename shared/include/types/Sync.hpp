#pragma once

#include <chrono>
#include <string>
#include <ctime>
#include <boost/beast/zlib/zlib.hpp>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
}

namespace vh::types {

struct Sync {
    enum class Strategy { Cache, Sync, Mirror };

    enum class ConflictPolicy {
        KeepLocal,
        KeepRemote,
        Ask
    };

    unsigned int id{}, vault_id{};
    std::chrono::seconds interval{};
    Strategy strategy{Strategy::Cache};
    ConflictPolicy conflict_policy{ConflictPolicy::KeepLocal};
    bool enabled{};
    std::time_t last_sync_at{}, last_success_at{}, created_at{}, updated_at{};

    Sync() = default;
    explicit Sync(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const Sync& s);
void from_json(const nlohmann::json& j, Sync& s);

void to_string(std::string& str, const Sync::Strategy& s);
void to_string(std::string& str, const Sync::ConflictPolicy& cp);

Sync::Strategy strategyFromString(const std::string& str);
Sync::ConflictPolicy conflictPolicyFromString(const std::string& str);

}

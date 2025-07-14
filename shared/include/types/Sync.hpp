#pragma once

#include <chrono>
#include <string>
#include <ctime>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
}

namespace vh::types {

struct Sync {
    unsigned int id{};
    std::chrono::seconds interval{};
    std::string conflict_policy, strategy;
    bool enabled{};
    std::time_t last_sync_at{}, last_success_at{}, created_at{}, updated_at{};

    Sync() = default;
    explicit Sync(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const Sync& s);
void from_json(const nlohmann::json& j, Sync& s);

}

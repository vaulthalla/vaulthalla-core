#pragma once

#include <string>
#include <ctime>
#include <cstdint>
#include <nlohmann/json_fwd.hpp>

namespace pqxx { class row; class result; }

namespace vh::rbac::role {

struct Base {
    uint32_t id{}, assignment_id{};
    std::string name, description, type;
    std::time_t created_at{}, updated_at{}, assigned_at{};

    Base() = default;
    explicit Base(const pqxx::row& row);
    explicit Base(const nlohmann::json& j);
    Base(std::string name, std::string description, std::string type, uint16_t permissions);

    virtual ~Base() = default;

    [[nodiscard]] virtual std::string permissions_to_flags_string() const;

    static std::string underscore_to_hyphens(const std::string& s);
};

void to_json(nlohmann::json& j, const Base& r);
void from_json(const nlohmann::json& j, Base& r);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Base>>& roles);

std::vector<std::shared_ptr<Base>> roles_from_pq_res(const pqxx::result& res);

std::string to_string(const std::shared_ptr<Base>& r);
std::string to_string(const std::vector<std::shared_ptr<Base>>& roles);

}

#pragma once

#include <string>
#include <optional>
#include <memory>
#include <vector>
#include <ctime>

#include <nlohmann/json_fwd.hpp> // Forward-decl only
#include <boost/describe.hpp>

namespace pqxx {
class row;
}

namespace vh::types {

enum class RoleName;

struct User {
    unsigned short id;
    std::string name;
    std::string email;
    std::string password_hash;
    std::time_t created_at;
    std::optional<std::time_t> last_login;
    bool is_active;
    RoleName role;

    User();
    User(std::string name, std::string email, bool isActive, const RoleName& role);
    explicit User(const pqxx::row& row);

    void setPasswordHash(const std::string& hash);

    [[nodiscard]] bool isAdmin() const;
    [[nodiscard]] bool isUser() const;
    [[nodiscard]] bool isGuest() const;
    [[nodiscard]] bool isModerator() const;
};

} // namespace vh::types

BOOST_DESCRIBE_STRUCT(vh::types::User, (), (id, name, email, password_hash, created_at, last_login, is_active))

namespace vh::types {

void to_json(nlohmann::json& j, const User& u);
void from_json(const nlohmann::json& j, User& u);

nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users);
nlohmann::json to_json(const std::shared_ptr<User>& user);

} // namespace vh::types

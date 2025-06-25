#pragma once

#include <string>
#include <optional>
#include <memory>
#include <vector>
#include <ctime>

#include <nlohmann/json_fwd.hpp> // Forward-decl only

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct UserRole;

struct User {
    unsigned short id;
    std::string name;
    std::string email;
    std::string password_hash;
    std::time_t created_at;
    std::optional<std::time_t> last_login;
    bool is_active;
    std::shared_ptr<UserRole> global_role; // Global role
    std::optional<std::vector<std::shared_ptr<UserRole>>> scoped_roles;  // Scoped roles, if any

    User();
    User(std::string name, std::string email, const bool isActive);
    explicit User(const pqxx::row& row);
    User(const pqxx::row& user, const pqxx::result& roles);

    void updateUser(const nlohmann::json& j);
    void setPasswordHash(const std::string& hash);

    [[nodiscard]] bool canManageUsers() const;
    [[nodiscard]] bool canManageRoles() const;
    [[nodiscard]] bool canManageStorage() const;
    [[nodiscard]] bool canManageFiles() const;
    [[nodiscard]] bool canViewAuditLog() const;
    [[nodiscard]] bool canUploadFile() const;
    [[nodiscard]] bool canDownloadFile() const;
    [[nodiscard]] bool canDeleteFile() const;
    [[nodiscard]] bool canShareFile() const;
    [[nodiscard]] bool canLockFile() const;
};

} // namespace vh::types

namespace vh::types {

void to_json(nlohmann::json& j, const User& u);
void from_json(const nlohmann::json& j, User& u);

nlohmann::json to_json(const std::vector<std::shared_ptr<User>>& users);
nlohmann::json to_json(const std::shared_ptr<User>& user);

} // namespace vh::types

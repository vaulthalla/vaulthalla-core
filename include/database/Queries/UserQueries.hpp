#pragma once

#include <memory>
#include <string>
#include <vector>

namespace vh::auth {
class RefreshToken;
}

namespace vh::types {
struct User;
struct BaseRole;
}

namespace vh::database {
class UserQueries {
  public:
    UserQueries() = default;

    [[nodiscard]] static std::shared_ptr<types::User> getUserByName(const std::string& name);
    [[nodiscard]] static std::shared_ptr<types::User> getUserById(unsigned int id);
    static unsigned int createUser(const std::shared_ptr<types::User>& user);
    static void updateUser(const std::shared_ptr<types::User>& user);
    static bool authenticateUser(const std::string& name, const std::string& password);
    static void updateUserPassword(unsigned int userId, const std::string& newPassword);
    static void deleteUser(unsigned int userId);
    static std::vector<std::shared_ptr<types::User>> listUsers();
    static void updateLastLoggedInUser(unsigned int userId);
    [[nodiscard]] static unsigned int getUserIdByLinuxUID(unsigned int linuxUid);
    [[nodiscard]] static std::shared_ptr<types::User> getUserByLinuxUID(unsigned int linuxUid);

    static void addRefreshToken(const std::shared_ptr<auth::RefreshToken>& token);
    static void removeRefreshToken(const std::string& jti);
    static std::shared_ptr<auth::RefreshToken> getRefreshToken(const std::string& jti);
    static std::vector<std::shared_ptr<auth::RefreshToken>> listRefreshTokens(unsigned int userId);
    static void revokeAllRefreshTokens(unsigned int userId);
    static void revokeAndPurgeRefreshTokens(unsigned int userId);
    static std::shared_ptr<types::User> getUserByRefreshToken(const std::string& jti);

    [[nodiscard]] static bool adminUserExists();
    [[nodiscard]] static bool adminPasswordIsDefault();
};
} // namespace vh::database

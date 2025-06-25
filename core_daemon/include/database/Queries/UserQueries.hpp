#pragma once

#include <memory>
#include <string>
#include <vector>

namespace vh::auth {
class RefreshToken;
}

namespace vh::types {
struct User;
struct Role;
}

namespace vh::database {
class UserQueries {
  public:
    UserQueries() = default;

    [[nodiscard]] static std::shared_ptr<types::User> getUserByEmail(const std::string& email);
    [[nodiscard]] static std::shared_ptr<types::User> getUserById(unsigned int id);
    static void createUser(const std::shared_ptr<types::User>& user);
    static void updateUser(const std::shared_ptr<types::User>& user);
    static bool authenticateUser(const std::string& email, const std::string& password);
    static void updateUserPassword(const std::string& email, const std::string& newPassword);
    static void deleteUser(const std::string& email);
    static std::vector<std::shared_ptr<types::User>> listUsers();
    static void updateLastLoggedInUser(unsigned int userId);

    static std::shared_ptr<types::Role> getRole(unsigned int id);

    static void addRefreshToken(const std::shared_ptr<auth::RefreshToken>& token);
    static void removeRefreshToken(const std::string& jti);
    static std::shared_ptr<auth::RefreshToken> getRefreshToken(const std::string& jti);
    static std::vector<std::shared_ptr<auth::RefreshToken>> listRefreshTokens(unsigned int userId);
    static void revokeAllRefreshTokens(unsigned int userId);
    static void revokeAndPurgeRefreshTokens(unsigned int userId);
    static std::shared_ptr<types::User> getUserByRefreshToken(const std::string& jti);
};
} // namespace vh::database

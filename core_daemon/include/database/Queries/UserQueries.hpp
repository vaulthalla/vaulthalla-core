#pragma once

#include <chrono>
#include <memory>
#include <string>

namespace vh::auth {
class RefreshToken;
}

namespace vh::types {
class User;
}

namespace vh::database {
class UserQueries {
  public:
    UserQueries() = default;

    [[nodiscard]] static std::shared_ptr<types::User> getUserByEmail(const std::string& email);
    static void createUser(const std::string& name, const std::string& email, const std::string& password_hash);
    static bool authenticateUser(const std::string& email, const std::string& password);
    static void updateUserPassword(const std::string& email, const std::string& newPassword);
    static void deleteUser(const std::string& email);

    static void addRefreshToken(const std::shared_ptr<auth::RefreshToken>& token);
    static void removeRefreshToken(const std::string& jti);
    static std::shared_ptr<auth::RefreshToken> getRefreshToken(const std::string& jti);
    static std::vector<std::shared_ptr<auth::RefreshToken>> listRefreshTokens(unsigned int userId);
    static void revokeAllRefreshTokens(unsigned int userId);
    static void revokeAndPurgeRefreshTokens(unsigned int userId);
    static std::shared_ptr<types::User> getUserByRefreshToken(const std::string& jti);
};
} // namespace vh::database

#pragma once

#include "database/model/ListQueryParams.hpp"

#include <memory>
#include <string>
#include <vector>

namespace vh::auth::model { class RefreshToken; }
namespace vh::identities::model { struct User; }

namespace vh::database {

class UserQueries {
  public:
    UserQueries() = default;

    [[nodiscard]] static std::shared_ptr<identities::model::User> getUserByName(const std::string& name);
    [[nodiscard]] static std::shared_ptr<identities::model::User> getUserById(unsigned int id);
    static unsigned int createUser(const std::shared_ptr<identities::model::User>& user);
    static void updateUser(const std::shared_ptr<identities::model::User>& user);
    static bool authenticateUser(const std::string& name, const std::string& password);
    static void updateUserPassword(unsigned int userId, const std::string& newPassword);
    static void deleteUser(unsigned int userId);
    static std::vector<std::shared_ptr<identities::model::User>> listUsers(model::ListQueryParams&& params = {});
    static void updateLastLoggedInUser(unsigned int userId);
    [[nodiscard]] static unsigned int getUserIdByLinuxUID(unsigned int linuxUid);
    [[nodiscard]] static std::shared_ptr<identities::model::User> getUserByLinuxUID(unsigned int linuxUid);

    static void addRefreshToken(const std::shared_ptr<auth::model::RefreshToken>& token);
    static void removeRefreshToken(const std::string& jti);
    static std::shared_ptr<auth::model::RefreshToken> getRefreshToken(const std::string& jti);
    static std::vector<std::shared_ptr<auth::model::RefreshToken>> listRefreshTokens(unsigned int userId);
    static void revokeAllRefreshTokens(unsigned int userId);
    static void revokeAndPurgeRefreshTokens(unsigned int userId);
    static std::shared_ptr<identities::model::User> getUserByRefreshToken(const std::string& jti);

    [[nodiscard]] static bool userExists(const std::string& name);
    [[nodiscard]] static bool adminUserExists();
    [[nodiscard]] static bool adminPasswordIsDefault();
};

}

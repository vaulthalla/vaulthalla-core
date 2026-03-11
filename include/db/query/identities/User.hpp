#pragma once

#include "db/model/ListQueryParams.hpp"

#include <memory>
#include <string>
#include <vector>

namespace vh::identities::model { struct Admin; }

namespace vh::db::query::identities {

class User {
    using U = vh::identities::model::Admin;
    using UserPtr = std::shared_ptr<U>;

public:
    User() = default;

    [[nodiscard]] static UserPtr getUserByName(const std::string& name);
    [[nodiscard]] static UserPtr getUserById(unsigned int id);
    static unsigned int createUser(const UserPtr& user);
    static void updateUser(const UserPtr& user);
    static bool authenticateUser(const std::string& name, const std::string& password);
    static void updateUserPassword(unsigned int userId, const std::string& newPassword);
    static void deleteUser(unsigned int userId);
    static std::vector<UserPtr> listUsers(model::ListQueryParams&& params = {});
    static void updateLastLoggedInUser(unsigned int userId);
    [[nodiscard]] static unsigned int getUserIdByLinuxUID(unsigned int linuxUid);
    [[nodiscard]] static UserPtr getUserByLinuxUID(unsigned int linuxUid);

    [[nodiscard]] static bool userExists(const std::string& name);
    [[nodiscard]] static bool adminUserExists();
    [[nodiscard]] static bool adminPasswordIsDefault();
};

}

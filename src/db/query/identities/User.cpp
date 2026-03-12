#include "db/query/identities/User.hpp"
#include "../../../../include/identities/User.hpp"
#include "auth/model/RefreshToken.hpp"
#include "db/Transactions.hpp"
#include "../../../../include/rbac/role/Admin.hpp"
#include "rbac/model/VaultRole.hpp"
#include "log/Registry.hpp"
#include "crypto/util/hash.hpp"

#include <pqxx/pqxx>

using namespace vh::db::model;
using namespace vh::db::query::identities;
using namespace vh::rbac::model;
using namespace vh::auth::model;

Admin::UserPtr Admin::getUserByName(const std::string& name) {
    return Transactions::exec("User::getUserByName", [&](pqxx::work& txn) -> UserPtr {
        const auto res = txn.exec(pqxx::prepped{"get_user_by_name"}, pqxx::params{name});
        if (res.empty()) {
            log::Registry::db()->trace("[User] No user found with name: {}", name);
            return nullptr;
        }
        const auto userRow = res.one_row();
        const auto userId = userRow["id"].as<unsigned int>();
        const auto userRoleRow = txn.exec(pqxx::prepped{"get_user_assigned_role"}, pqxx::params{userId}).one_row();

        const auto rolesRes = txn.exec(pqxx::prepped{"get_user_and_group_assigned_vault_roles"}, userId);
        const auto overridesRes = txn.exec(pqxx::prepped{"list_user_and_group_permission_overrides"}, userId);

        return std::make_shared<U>(userRow, userRoleRow, rolesRes, overridesRes);
    });
}

Admin::UserPtr Admin::getUserById(const unsigned int id) {
    return Transactions::exec("User::getUserById", [&](pqxx::work& txn) {
        const auto userRow = txn.exec(pqxx::prepped{"get_user"}, pqxx::params{id}).one_row();
        const auto userRoleRow = txn.exec(pqxx::prepped{"get_user_assigned_role"}, pqxx::params{id}).one_row();

        const auto rolesRes = txn.exec(pqxx::prepped{"get_user_and_group_assigned_vault_roles"}, id);
        const auto overridesRes = txn.exec(pqxx::prepped{"list_user_and_group_permission_overrides"}, id);

        return std::make_shared<U>(userRow, userRoleRow, rolesRes, overridesRes);
    });
}

Admin::UserPtr Admin::getUserByLinuxUID(unsigned int linuxUid) {
    return Transactions::exec("User::getUserByLinuxUID", [&](pqxx::work& txn) -> UserPtr {
        const pqxx::result res = txn.exec(pqxx::prepped{"get_user_by_linux_uid"}, pqxx::params{linuxUid});
        if (res.empty()) {
            log::Registry::db()->debug("[User] No user found with Linux UID: {}", linuxUid);
            return nullptr;
        }
        const auto userRow = res.one_row();
        const auto userId = userRow["id"].as<unsigned int>();
        const auto userRoleRow = txn.exec(pqxx::prepped{"get_user_assigned_role"}, pqxx::params{userId}).one_row();

        const auto rolesRes = txn.exec(pqxx::prepped{"get_user_and_group_assigned_vault_roles"}, userId);
        const auto overridesRes = txn.exec(pqxx::prepped{"list_user_and_group_permission_overrides"}, userId);

        return std::make_shared<U>(userRow, userRoleRow, rolesRes, overridesRes);
    });
}

unsigned int Admin::getUserIdByLinuxUID(const unsigned int linuxUid) {
    return Transactions::exec("User::getUserIdByLinuxUID", [&](pqxx::work& txn) {
        const pqxx::result res = txn.exec(pqxx::prepped{"get_user_id_by_linux_uid"}, pqxx::params{linuxUid});
        if (res.empty()) throw std::runtime_error("No user found with Linux UID: " + std::to_string(linuxUid));
        return res.one_field().as<unsigned int>();
    });
}

unsigned int Admin::createUser(const UserPtr& user) {
    log::Registry::db()->debug("[User] Creating user: {}", user->name);
    if (!user->role) throw std::runtime_error("User role must be set before creating a user");
    return Transactions::exec("User::createUser", [&](pqxx::work& txn) {
        pqxx::params p{
            user->name,
            user->email,
            user->password_hash,
            user->is_active,
            user->linux_uid,
            user->last_modified_by
        };
        const auto userId = txn.exec(pqxx::prepped{"insert_user"}, p).one_row()[0].as<unsigned int>();

        txn.exec(pqxx::prepped{"assign_user_role"}, pqxx::params{userId, user->role->id});

        for (const auto& [_, role] : user->roles) {
            pqxx::params role_params{"user", userId, role->vault_id, role->role_id};
            const auto res = txn.exec(pqxx::prepped{"assign_vault_role"}, role_params);
            if (res.empty()) continue;
            role->assignment_id = res.one_field().as<unsigned int>();

            for (const auto& override : role->permission_overrides) {
                override->assignment_id = role->assignment_id;
                pqxx::params overrideParams{
                    override->assignment_id,
                    override->permission.id,
                    override->patternStr,
                    override->enabled,
                    to_string(override->effect)
                };
                txn.exec(pqxx::prepped{"insert_vault_permission_override"}, overrideParams);
            }
        }

        return userId;
    });
}

void Admin::updateUser(const UserPtr& user) {
    Transactions::exec("User::updateUser", [&](pqxx::work& txn) {
        pqxx::params u_params{
            user->id,
            user->name,
            user->email,
            user->password_hash,
            user->is_active,
            user->linux_uid,
            user->last_modified_by
        };
        txn.exec(pqxx::prepped{"update_user"}, u_params);

        if (user->role->id != txn.exec(pqxx::prepped{"get_user_role_id"}, pqxx::params{user->id}).one_field().as<unsigned int>())
            txn.exec(pqxx::prepped{"update_user_role"}, pqxx::params{user->id, user->role->id});
    });
}

bool Admin::authenticateUser(const std::string& name, const std::string& password) {
    return Transactions::exec("User::authenticateUser", [&](pqxx::work& txn) -> bool {
        const pqxx::result res = txn.exec("SELECT password_hash FROM users WHERE name = " + txn.quote(name));
        if (res.empty()) return false; // User not found
        const std::string& storedHash = res[0][0].as<std::string>();
        return storedHash == password;
    });
}

void Admin::updateUserPassword(const unsigned int userId, const std::string& newPassword) {
    Transactions::exec("User::updateUserPassword", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"update_user_password"}, pqxx::params{userId, newPassword});
    });
}

void Admin::deleteUser(const unsigned int userId) {
    Transactions::exec("User::deleteUser", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM users WHERE id = " + txn.quote(userId));
    });
}

std::vector<Admin::UserPtr > Admin::listUsers(ListQueryParams&& params) {
    return Transactions::exec("User::listUsersWithRoles", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT * FROM users",
            params, "id", "name"
        );

        const auto res = txn.exec(sql);
        std::vector<UserPtr> usersWithRoles;

        for (const auto& row : res) {
            const auto userRoleRow = txn.exec(pqxx::prepped{"get_user_assigned_role"}, pqxx::params{row["id"].as<unsigned int>()}).one_row();
            pqxx::params p{"user", row["id"].as<unsigned int>()};
            const auto rolesRes = txn.exec(pqxx::prepped{"get_subject_assigned_vault_roles"}, p);
            const auto overridesRes = txn.exec(pqxx::prepped{"list_subject_permission_overrides"}, p);

            usersWithRoles.push_back(std::make_shared<U>(row, userRoleRow, rolesRes, overridesRes));
        }
        return usersWithRoles;
    });
}

void Admin::updateLastLoggedInUser(const unsigned int userId) {
    Transactions::exec("User::updateLastLoggedInUser", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"update_user_last_login"}, pqxx::params{userId});
    });
}

bool Admin::userExists(const std::string& name) {
    return Transactions::exec("User::userExists", [&](pqxx::work& txn) {
        return txn.exec(pqxx::prepped{"user_exists"}, pqxx::params{name}).one_field().as<bool>();
    });
}

bool Admin::adminUserExists() {
    return Transactions::exec("User::adminUserExists", [](pqxx::work& txn) {
        return txn.exec(pqxx::prepped{"admin_user_exists"}).one_field().as<bool>();
    });
}

bool Admin::adminPasswordIsDefault() {
    return Transactions::exec("User::adminPasswordIsDefault", [](pqxx::work& txn) {
        const auto passwordHash = txn.exec(pqxx::prepped{"get_admin_password"}).one_field().as<std::string>();
        return crypto::hash::verifyPassword("vh!adm1n", passwordHash);
    });
}

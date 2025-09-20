#include "database/Queries/UserQueries.hpp"
#include "types/User.hpp"
#include "types/UserRole.hpp"
#include "types/RefreshToken.hpp"
#include "database/Transactions.hpp"
#include "types/VaultRole.hpp"
#include "logging/LogRegistry.hpp"
#include "crypto/PasswordHash.hpp"

#include <pqxx/pqxx>

using namespace vh::logging;
using namespace vh::types;

namespace vh::database {

std::shared_ptr<User> UserQueries::getUserByName(const std::string& name) {
    return Transactions::exec("UserQueries::getUserByName", [&](pqxx::work& txn) -> std::shared_ptr<User> {
        const auto res = txn.exec(pqxx::prepped{"get_user_by_name"}, pqxx::params{name});
        if (res.empty()) {
            LogRegistry::db()->trace("[UserQueries] No user found with name: {}", name);
            return nullptr;
        }
        const auto userRow = res.one_row();
        const auto userId = userRow["id"].as<unsigned int>();
        const auto userRoleRow = txn.exec(pqxx::prepped{"get_user_assigned_role"}, pqxx::params{userId}).one_row();

        const auto rolesRes = txn.exec(pqxx::prepped{"get_user_and_group_assigned_vault_roles"}, userId);
        const auto overridesRes = txn.exec(pqxx::prepped{"list_user_and_group_permission_overrides"}, userId);

        return std::make_shared<User>(userRow, userRoleRow, rolesRes, overridesRes);
    });
}

std::shared_ptr<User> UserQueries::getUserById(const unsigned int id) {
    return Transactions::exec("UserQueries::getUserById", [&](pqxx::work& txn) {
        const auto userRow = txn.exec(pqxx::prepped{"get_user"}, pqxx::params{id}).one_row();
        const auto userRoleRow = txn.exec(pqxx::prepped{"get_user_assigned_role"}, pqxx::params{id}).one_row();

        const auto rolesRes = txn.exec(pqxx::prepped{"get_user_and_group_assigned_vault_roles"}, id);
        const auto overridesRes = txn.exec(pqxx::prepped{"list_user_and_group_permission_overrides"}, id);

        return std::make_shared<User>(userRow, userRoleRow, rolesRes, overridesRes);
    });
}

std::shared_ptr<User> UserQueries::getUserByRefreshToken(const std::string& jti) {
    return Transactions::exec("UserQueries::getUserByRefreshToken", [&](pqxx::work& txn) -> std::shared_ptr<User> {
        const auto res = txn.exec(pqxx::prepped{"get_user_by_refresh_token"}, pqxx::params{jti});
        if (res.empty()) {
            LogRegistry::db()->trace("[UserQueries] No user found for refresh token JTI: {}", jti);
            return nullptr;
        }

        const auto userRow = res.one_row();
        const auto userId = userRow["id"].as<unsigned int>();
        const auto userRoleRow = txn.exec(pqxx::prepped{"get_user_assigned_role"}, pqxx::params{userId}).one_row();

        const auto rolesRes = txn.exec(pqxx::prepped{"get_user_and_group_assigned_vault_roles"}, userId);
        const auto overridesRes = txn.exec(pqxx::prepped{"list_user_and_group_permission_overrides"}, userId);

        return std::make_shared<User>(userRow, userRoleRow, rolesRes, overridesRes);
    });
}

unsigned int UserQueries::createUser(const std::shared_ptr<User>& user) {
    LogRegistry::db()->debug("[UserQueries] Creating user: {}", user->name);
    if (!user->role) throw std::runtime_error("User role must be set before creating a user");
    return Transactions::exec("UserQueries::createUser", [&](pqxx::work& txn) {
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

        for (const auto& [_, role] : user->group_roles) {
            pqxx::params role_params{"user", userId, role->vault_id, role->role_id};
            const auto res = txn.exec(pqxx::prepped{"assign_vault_role_to_user_from_group"}, role_params);
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

void UserQueries::updateUser(const std::shared_ptr<User>& user) {
    Transactions::exec("UserQueries::updateUser", [&](pqxx::work& txn) {
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

bool UserQueries::authenticateUser(const std::string& name, const std::string& password) {
    return Transactions::exec("UserQueries::authenticateUser", [&](pqxx::work& txn) -> bool {
        const pqxx::result res = txn.exec("SELECT password_hash FROM users WHERE name = " + txn.quote(name));
        if (res.empty()) return false; // User not found
        const std::string& storedHash = res[0][0].as<std::string>();
        return storedHash == password;
    });
}

void UserQueries::updateUserPassword(const unsigned int userId, const std::string& newPassword) {
    Transactions::exec("UserQueries::updateUserPassword", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"update_user_password"}, pqxx::params{userId, newPassword});
    });
}

void UserQueries::deleteUser(const unsigned int userId) {
    Transactions::exec("UserQueries::deleteUser", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM users WHERE id = " + txn.quote(userId));
    });
}

unsigned int UserQueries::getUserIdByLinuxUID(const unsigned int linuxUid) {
    return Transactions::exec("UserQueries::getUserIdByLinuxUID", [&](pqxx::work& txn) {
        const pqxx::result res = txn.exec(pqxx::prepped{"get_user_id_by_linux_uid"}, pqxx::params{linuxUid});
        if (res.empty()) throw std::runtime_error("No user found with Linux UID: " + std::to_string(linuxUid));
        return res.one_field().as<unsigned int>();
    });
}

std::shared_ptr<User> UserQueries::getUserByLinuxUID(unsigned int linuxUid) {
    return Transactions::exec("UserQueries::getUserByLinuxUID", [&](pqxx::work& txn) -> std::shared_ptr<User> {
        const pqxx::result res = txn.exec(pqxx::prepped{"get_user_by_linux_uid"}, pqxx::params{linuxUid});
        if (res.empty()) {
            LogRegistry::db()->error("[UserQueries] No user found with Linux UID: {}", linuxUid);
            return nullptr;
        }
        const auto userRow = res.one_row();
        const auto userId = userRow["id"].as<unsigned int>();
        const auto userRoleRow = txn.exec(pqxx::prepped{"get_user_assigned_role"}, pqxx::params{userId}).one_row();

        pqxx::params p{"user", userId};
        const auto rolesRes = txn.exec(pqxx::prepped{"get_subject_assigned_vault_roles"}, p);
        const auto overridesRes = txn.exec(pqxx::prepped{"list_subject_permission_overrides"}, p);

        return std::make_shared<User>(userRow, userRoleRow, rolesRes, overridesRes);
    });
}

std::vector<std::shared_ptr<User> > UserQueries::listUsers(ListQueryParams&& params) {
    return Transactions::exec("UserQueries::listUsersWithRoles", [&](pqxx::work& txn) {
        const auto sql = appendPaginationAndFilter(
            "SELECT * FROM users",
            params, "id", "name"
        );

        const auto res = txn.exec(sql);
        std::vector<std::shared_ptr<User>> usersWithRoles;

        for (const auto& row : res) {
            const auto userRoleRow = txn.exec(pqxx::prepped{"get_user_assigned_role"}, pqxx::params{row["id"].as<unsigned int>()}).one_row();
            pqxx::params p{"user", row["id"].as<unsigned int>()};
            const auto rolesRes = txn.exec(pqxx::prepped{"get_subject_assigned_vault_roles"}, p);
            const auto overridesRes = txn.exec(pqxx::prepped{"list_subject_permission_overrides"}, p);

            usersWithRoles.push_back(std::make_shared<User>(row, userRoleRow, rolesRes, overridesRes));
        }
        return usersWithRoles;
    });
}

void UserQueries::updateLastLoggedInUser(const unsigned int userId) {
    Transactions::exec("UserQueries::updateLastLoggedInUser", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"update_user_last_login"}, pqxx::params{userId});
    });
}

void UserQueries::addRefreshToken(const std::shared_ptr<auth::RefreshToken>& token) {
    Transactions::exec("UserQueries::addRefreshToken", [&](pqxx::work& txn) {
        pqxx::params p{token->getJti(), token->getUserId(), token->getHashedToken(), token->getIpAddress(),
                       token->getUserAgent()};
        txn.exec(pqxx::prepped{"insert_refresh_token"}, p);
    });
}

void UserQueries::removeRefreshToken(const std::string& jti) {
    Transactions::exec("UserQueries::removeRefreshToken", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM refresh_tokens WHERE jti = " + txn.quote(jti));
    });
}

std::shared_ptr<auth::RefreshToken> UserQueries::getRefreshToken(const std::string& jti) {
    return Transactions::exec("UserQueries::getRefreshToken", [&](pqxx::work& txn) -> std::shared_ptr<auth::RefreshToken> {
        const auto res = txn.exec("SELECT * FROM refresh_tokens WHERE jti = " + txn.quote(jti));
        if (res.empty()) {
            LogRegistry::db()->trace("[UserQueries] No refresh token found for JTI: {}", jti);
            return nullptr;
        }
        return std::make_shared<auth::RefreshToken>(res.one_row());
    });
}

std::vector<std::shared_ptr<auth::RefreshToken> > UserQueries::listRefreshTokens(const unsigned int userId) {
    return Transactions::exec("UserQueries::listRefreshTokens", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT * FROM refresh_tokens WHERE user_id = " + txn.quote(userId));
        std::vector<std::shared_ptr<auth::RefreshToken> > tokens;
        for (const auto& row : res) tokens.push_back(std::make_shared<vh::auth::RefreshToken>(row));
        return tokens;
    });
}

void UserQueries::revokeAllRefreshTokens(const unsigned int userId) {
    Transactions::exec("UserQueries::revokeAllRefreshTokens", [&](pqxx::work& txn) {
        txn.exec("UPDATE refresh_tokens SET revoked = TRUE WHERE user_id = " + txn.quote(userId));
    });
}

void UserQueries::revokeAndPurgeRefreshTokens(const unsigned int userId) {
    Transactions::exec("UserQueries::revokeAndPurgeRefreshTokens", [&](pqxx::work& txn) {
        pqxx::params p{userId};
        txn.exec(pqxx::prepped{"revoke_most_recent_refresh_token"}, p);
        txn.exec(pqxx::prepped{"delete_refresh_tokens_older_than_7_days"}, p);
        txn.exec(pqxx::prepped{"delete_refresh_tokens_keep_five"}, p);
    });
}

bool UserQueries::userExists(const std::string& name) {
    return Transactions::exec("UserQueries::userExists", [&](pqxx::work& txn) {
        return txn.exec(pqxx::prepped{"user_exists"}, pqxx::params{name}).one_field().as<bool>();
    });
}

bool UserQueries::adminUserExists() {
    return Transactions::exec("UserQueries::adminUserExists", [](pqxx::work& txn) {
        return txn.exec(pqxx::prepped{"admin_user_exists"}).one_field().as<bool>();
    });
}

bool UserQueries::adminPasswordIsDefault() {
    return Transactions::exec("UserQueries::adminPasswordIsDefault", [](pqxx::work& txn) {
        const auto passwordHash = txn.exec(pqxx::prepped{"get_admin_password"}).one_field().as<std::string>();
        return crypto::verifyPassword("vh!adm1n", passwordHash);
    });
}

} // namespace vh::database
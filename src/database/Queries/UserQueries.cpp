#include "database/Queries/UserQueries.hpp"
#include "types/User.hpp"
#include "types/UserRole.hpp"
#include "types/RefreshToken.hpp"
#include "database/Transactions.hpp"
#include "types/VaultRole.hpp"
#include <pqxx/pqxx>

namespace vh::database {

std::shared_ptr<types::User> UserQueries::getUserByName(const std::string& name) {
    return Transactions::exec("UserQueries::getUserByName", [&](pqxx::work& txn) {
        const auto userRow = txn.exec_prepared("get_user_by_name", pqxx::params{name}).one_row();
        const auto userId = userRow["id"].as<unsigned int>();
        const auto userRoleRow = txn.exec_prepared("get_user_assigned_role", pqxx::params{userId}).one_row();

        pqxx::params p{"user", userId};
        const auto rolesRes = txn.exec_prepared("get_subject_assigned_vault_roles", p);
        const auto overridesRes = txn.exec_prepared("get_subject_permission_overrides", p);

        return std::make_shared<types::User>(userRow, userRoleRow, rolesRes, overridesRes);
    });
}

std::shared_ptr<types::User> UserQueries::getUserById(const unsigned int id) {
    return Transactions::exec("UserQueries::getUserById", [&](pqxx::work& txn) {
        const auto userRow = txn.exec_prepared("get_user", pqxx::params{id}).one_row();
        const auto userRoleRow = txn.exec_prepared("get_user_assigned_role", pqxx::params{id}).one_row();

        pqxx::params p{"user", id};
        const auto rolesRes = txn.exec_prepared("get_subject_assigned_vault_roles", p);
        const auto overridesRes = txn.exec_prepared("get_subject_permission_overrides", p);

        return std::make_shared<types::User>(userRow, userRoleRow, rolesRes, overridesRes);
    });
}

std::shared_ptr<types::User> UserQueries::getUserByRefreshToken(const std::string& jti) {
    return Transactions::exec("UserQueries::getUserByRefreshToken", [&](pqxx::work& txn) {
        const auto userRow = txn.exec_prepared("get_user_by_refresh_token", pqxx::params{jti}).one_row();
        const auto userId = userRow["id"].as<unsigned int>();
        const auto userRoleRow = txn.exec_prepared("get_user_assigned_role", pqxx::params{userId}).one_row();

        pqxx::params p{"user", userId};
        const auto rolesRes = txn.exec_prepared("get_subject_assigned_vault_roles", p);
        const auto overridesRes = txn.exec_prepared("get_subject_permission_overrides", p);

        return std::make_shared<types::User>(userRow, userRoleRow, rolesRes, overridesRes);
    });
}

void UserQueries::createUser(const std::shared_ptr<types::User>& user) {
    if (!user->role) throw std::runtime_error("User role must be set before creating a user");
    Transactions::exec("UserQueries::createUser", [&](pqxx::work& txn) {
        pqxx::params p{user->name, user->email, user->password_hash, user->is_active};
        const auto userId = txn.exec_prepared("insert_user", p).one_row()[0].as<unsigned int>();

        txn.exec_prepared("assign_user_role", pqxx::params{userId, user->role->role_id});

        for (const auto& role : user->roles) {
            pqxx::params role_params{"user", role->vault_id, userId, role->role_id};
            txn.exec_prepared("assign_vault_role", role_params);
        }
    });
}

void UserQueries::updateUser(const std::shared_ptr<types::User>& user) {
    Transactions::exec("UserQueries::updateUser", [&](pqxx::work& txn) {
        pqxx::params u_params{user->id, user->name, user->email, user->password_hash};
        txn.exec_prepared("update_user", u_params);

        const auto existingRoleRow = txn.exec_prepared("get_user_assigned_role", pqxx::params{user->id}).one_row();
        const auto existingRoleId = existingRoleRow["role_id"].as<unsigned int>();

        if (user->role->role_id != existingRoleId)
            txn.exec_prepared("update_user_role", pqxx::params{user->id, user->role->role_id});
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
        txn.exec_prepared("update_user_password", pqxx::params{userId, newPassword});
    });
}

void UserQueries::deleteUser(const unsigned int userId) {
    Transactions::exec("UserQueries::deleteUser", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM users WHERE id = " + txn.quote(userId));
    });
}

std::vector<std::shared_ptr<types::User> > UserQueries::listUsers() {
    return Transactions::exec("UserQueries::listUsersWithRoles", [&](pqxx::work& txn) {
        const pqxx::result res = txn.exec_prepared("get_users");

        std::vector<std::shared_ptr<types::User>> usersWithRoles;

        for (const auto& row : res) {
            const auto userRoleRow = txn.exec_prepared("get_user_assigned_role", pqxx::params{row["id"].as<unsigned int>()}).one_row();
            pqxx::params p{"user", row["id"].as<unsigned int>()};
            const auto rolesRes = txn.exec_prepared("get_subject_assigned_vault_roles", p);
            const auto overridesRes = txn.exec_prepared("get_subject_permission_overrides", p);

            usersWithRoles.push_back(std::make_shared<types::User>(row, userRoleRow, rolesRes, overridesRes));
        }
        return usersWithRoles;
    });
}

void UserQueries::updateLastLoggedInUser(const unsigned int userId) {
    Transactions::exec("UserQueries::updateLastLoggedInUser", [&](pqxx::work& txn) {
        txn.exec_prepared("update_user_last_login", pqxx::params{userId});
    });
}

void UserQueries::addRefreshToken(const std::shared_ptr<auth::RefreshToken>& token) {
    Transactions::exec("UserQueries::addRefreshToken", [&](pqxx::work& txn) {
        pqxx::params p{token->getJti(), token->getUserId(), token->getHashedToken(), token->getIpAddress(),
                       token->getUserAgent()};
        txn.exec_prepared("insert_refresh_token", p);
    });
}

void UserQueries::removeRefreshToken(const std::string& jti) {
    Transactions::exec("UserQueries::removeRefreshToken", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM refresh_tokens WHERE jti = " + txn.quote(jti));
    });
}

std::shared_ptr<auth::RefreshToken> UserQueries::getRefreshToken(const std::string& jti) {
    return Transactions::exec("UserQueries::getRefreshToken", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT * FROM refresh_tokens WHERE jti = " + txn.quote(jti));
        return std::make_shared<auth::RefreshToken>(res[0]);
    });
}

std::vector<std::shared_ptr<auth::RefreshToken> > UserQueries::listRefreshTokens(const unsigned int userId) {
    return Transactions::exec("UserQueries::listRefreshTokens", [&](pqxx::work& txn) {
        const auto res = txn.exec("SELECT * FROM refresh_tokens WHERE user_id = " + txn.quote(userId));
        std::vector<std::shared_ptr<auth::RefreshToken> > tokens;
        for (const auto& row : res) {
            tokens.push_back(std::make_shared<vh::auth::RefreshToken>(row));
        }
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
        txn.exec_prepared("revoke_most_recent_refresh_token", p);
        txn.exec_prepared("delete_refresh_tokens_older_than_7_days", p);
        txn.exec_prepared("delete_refresh_tokens_keep_five", p);
    });
}

} // namespace vh::database
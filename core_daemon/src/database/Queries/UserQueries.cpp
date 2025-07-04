#include "database/Queries/UserQueries.hpp"
#include "types/User.hpp"
#include "auth/RefreshToken.hpp"
#include "database/Transactions.hpp"
#include "types/AssignedRole.hpp"
#include <pqxx/pqxx>

namespace vh::database {

std::shared_ptr<types::User> UserQueries::getUserByName(const std::string& name) {
    return Transactions::exec("UserQueries::getUserByName", [&](pqxx::work& txn) {
        const auto userRow = txn.exec("SELECT * FROM users WHERE name = " + txn.quote(name)).one_row();

        pqxx::params p{"user", userRow["id"].as<unsigned int>()};
        const auto rolesRes = txn.exec_prepared("get_subject_assigned_roles", p);
        const auto overridesRes = txn.exec_prepared("get_subject_permission_overrides", p);

        return std::make_shared<types::User>(userRow, rolesRes, overridesRes);
    });
}

std::shared_ptr<types::User> UserQueries::getUserById(const unsigned int id) {
    return Transactions::exec("UserQueries::getUserById", [&](pqxx::work& txn) {
        const auto userRow = txn.exec("SELECT * FROM users WHERE id = " + txn.quote(id)).one_row();

        pqxx::params p{"user", userRow["id"].as<unsigned int>()};
        const auto rolesRes = txn.exec_prepared("get_subject_assigned_roles", p);
        const auto overridesRes = txn.exec_prepared("get_subject_permission_overrides", p);

        return std::make_shared<types::User>(userRow, rolesRes, overridesRes);
    });
}

std::shared_ptr<types::User> UserQueries::getUserByRefreshToken(const std::string& jti) {
    return Transactions::exec("UserQueries::getUserByRefreshToken", [&](pqxx::work& txn) {
        const auto userRow = txn.exec_prepared("get_user_by_refresh_token", pqxx::params{jti}).one_row();

        pqxx::params p{"user", userRow["id"].as<unsigned int>()};
        const auto rolesRes = txn.exec_prepared("get_subject_assigned_roles", p);
        const auto overridesRes = txn.exec_prepared("get_subject_permission_overrides", p);

        return std::make_shared<types::User>(userRow, rolesRes, overridesRes);
    });
}

void UserQueries::createUser(const std::shared_ptr<types::User>& user) {
    Transactions::exec("UserQueries::createUser", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(user->name);
        p.append(user->email);
        p.append(user->password_hash);
        p.append(user->is_active);
        p.append(user->permissions);

        const auto userId = txn.exec_prepared("insert_user", p).one_row()[0].as<unsigned int>();

        for (const auto& role : user->roles) {
            pqxx::params role_params{"user", role->vault_id, userId, role->id};
            txn.exec_prepared("assign_role", role_params);
        }
    });
}

void UserQueries::updateUser(const std::shared_ptr<types::User>& user) {
    Transactions::exec("UserQueries::updateUser", [&](pqxx::work& txn) {
        pqxx::params u_params{user->id, user->name, user->email, user->password_hash, user->permissions};
        txn.exec_prepared("update_user", u_params);
    });
}

bool UserQueries::authenticateUser(const std::string& email, const std::string& password) {
    return Transactions::exec("UserQueries::authenticateUser", [&](pqxx::work& txn) -> bool {
        const pqxx::result res = txn.exec("SELECT password_hash FROM users WHERE email = " + txn.quote(email));
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
        const pqxx::result res = txn.exec(R"(SELECT * FROM users)");

        std::vector<std::shared_ptr<types::User> > usersWithRoles;

        for (const auto& row : res) {
            pqxx::params p{"user", row["id"].as<unsigned int>()};
            const auto rolesRes = txn.exec_prepared("get_subject_assigned_roles", p);
            const auto overridesRes = txn.exec_prepared("get_subject_permission_overrides", p);

            usersWithRoles.push_back(std::make_shared<types::User>(row, rolesRes, overridesRes));
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
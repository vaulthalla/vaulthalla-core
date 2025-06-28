#include "database/Queries/UserQueries.hpp"
#include "types/db/User.hpp"
#include "auth/RefreshToken.hpp"
#include "database/Transactions.hpp"
#include "types/db/Role.hpp"
#include "database/utils.hpp"
#include <pqxx/pqxx>

#include <set>

namespace vh::database {

static constexpr const char* ROLE_SELECT =
    "SELECT "
    "   rs.id                         AS id, "          // assignment PK (becomes Role.id)
    "   rs.role_id, "
    "   rs.subject_id, rs.scope, rs.scope_id, rs.assigned_at, rs.inherited, "
    "   r.name, r.display_name, r.description, r.created_at, "
    "   r.admin_permissions::int      AS admin_permissions, "
    "   r.vault_permissions::int      AS vault_permissions, "
    "   r.file_permissions::int       AS file_permissions, "
    "   r.directory_permissions::int  AS directory_permissions "
    "FROM role r JOIN roles rs ON r.id = rs.role_id ";

std::shared_ptr<types::User> UserQueries::getUserByEmail(const std::string& email) {
    return Transactions::exec("UserQueries::getUserByEmail", [&](pqxx::work& txn) {
        auto userRow = txn.exec("SELECT * FROM users WHERE email = " + txn.quote(email)).one_row();
        auto rolesRes = txn.exec(std::string(ROLE_SELECT) +
                                 "WHERE rs.subject_type = 'user' AND rs.subject_id = " + txn.quote(userRow["id"].as<unsigned int>()));
        return std::make_shared<vh::types::User>(userRow, rolesRes);
    });
}

std::shared_ptr<types::User> UserQueries::getUserById(unsigned int id) {
    return Transactions::exec("UserQueries::getUserById", [&](pqxx::work& txn) {
        auto userRow = txn.exec("SELECT * FROM users WHERE id = " + txn.quote(id)).one_row();
        auto rolesRes = txn.exec(std::string(ROLE_SELECT) +
                                 "WHERE rs.subject_type = 'user' AND rs.subject_id = " + txn.quote(id));
        return std::make_shared<vh::types::User>(userRow, rolesRes);
    });
}

std::shared_ptr<types::User> UserQueries::getUserByRefreshToken(const std::string& jti) {
    return Transactions::exec("UserQueries::getUserByRefreshToken", [&](pqxx::work& txn) {
        auto userRow = txn.exec(
            "SELECT u.* FROM users u JOIN refresh_tokens rt ON u.id = rt.user_id "
            "WHERE rt.jti = " + txn.quote(jti)).one_row();
        auto rolesRes = txn.exec(std::string(ROLE_SELECT) +
                                 "WHERE rs.subject_type = 'user' AND rs.subject_id = " + txn.quote(userRow["id"].as<unsigned int>()));
        return std::make_shared<vh::types::User>(userRow, rolesRes);
    });
}

void UserQueries::createUser(const std::shared_ptr<types::User>& user, const unsigned int roleId) {
    Transactions::exec("UserQueries::createUser", [&](pqxx::work& txn) {
        const auto userId = txn.exec("INSERT INTO users (name, email, password_hash, is_active) VALUES (" +
                                     txn.quote(user->name) + ", " + txn.quote(user->email) + ", " + txn.quote(
                                         user->password_hash)
                                     + ", " + txn.quote(user->is_active) + ") RETURNING id").one_row()[0].as<unsigned
            int>();

        txn.exec("INSERT INTO roles (user_id, role_id) VALUES (" + txn.quote(userId) + ", " +
                 txn.quote(roleId) + ")");

        if (user->scoped_roles.has_value()) {
            for (const auto& role : *user->scoped_roles) {
                txn.exec("INSERT INTO roles (user_id, role_id, scope, scope_id) VALUES (" +
                         txn.quote(userId) + ", " + txn.quote(role->id) + ", " + txn.quote(role->scope) +
                         ", " + txn.quote(role->scope_id) + ")");
            }
        }
    });
}

void UserQueries::updateUser(const std::shared_ptr<types::User>& user) {
    Transactions::exec("UserQueries::updateUser", [&](pqxx::work& txn) {
        // Update user info
        txn.exec("UPDATE users SET name = " + txn.quote(user->name) +
                 ", email = " + txn.quote(user->email) +
                 ", is_active = " + txn.quote(user->is_active) +
                 " WHERE id = " + txn.quote(user->id));

        std::set<std::tuple<int, std::string, std::optional<int> > > new_roles;

        // Add global role
        new_roles.insert({user->global_role->id, "global", std::nullopt});

        // Scoped roles
        if (user->scoped_roles.has_value()) {
            for (const auto& role : *user->scoped_roles) {
                new_roles.insert({role->id, role->scope, role->scope_id});
            }
        }

        // Upsert new roles
        for (const auto& [role_id, scope, scoped_id] : new_roles) {
            txn.exec("INSERT INTO roles (user_id, role_id, scope, scoped_id) VALUES (" +
                     txn.quote(user->id) + ", " + txn.quote(role_id) + ", " +
                     txn.quote(scope) + ", " + txn.quote(scoped_id) +
                     ") ON CONFLICT DO NOTHING");
        }

        // Delete roles not in the new set
        const std::string role_filter = "('" + std::to_string(user->id) + "', " +
                                        // Build a VALUES clause like: (role_id, scope, scoped_id)
                                        buildRoleValuesList(new_roles) + ")";

        txn.exec("DELETE FROM roles WHERE user_id = " + txn.quote(user->id) +
                 " AND (role_id, scope, scoped_id) NOT IN " +
                 "(VALUES " + role_filter + ")");
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

void UserQueries::updateUserPassword(const std::string& email, const std::string& newPassword) {
    Transactions::exec("UserQueries::updateUserPassword", [&](pqxx::work& txn) {
        txn.exec("UPDATE users SET password_hash = " + txn.quote(newPassword) + " WHERE email = " + txn.quote(email));
    });
}

void UserQueries::deleteUser(const std::string& email) {
    Transactions::exec("UserQueries::deleteUser", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM users WHERE email = " + txn.quote(email));
    });
}

std::vector<std::shared_ptr<types::User> > UserQueries::listUsers() {
    return Transactions::exec("UserQueries::listUsersWithRoles",
                              [&](pqxx::work& txn) -> std::vector<std::shared_ptr<types::User> > {
                                  const pqxx::result res = txn.exec(R"(SELECT u.*, r.* as role
                                                                FROM users u
                                                                JOIN roles rs ON u.id = rs.subject_id
                                                                JOIN role r ON rs.role_id = r.id)");

                                  std::vector<std::shared_ptr<types::User> > usersWithRoles;
                                  for (const auto& row : res) {

                                      const auto roles = txn.exec(
                                          "SELECT r.id, r.name, r.display_name, r.description, "
                                          "r.permissions::int AS permissions, r.created_at, "
                                          "r.*, rs.* "
                                          "FROM role r "
                                          "JOIN roles rs ON r.id = rs.role_id "
                                          "WHERE rs.subject_type = 'user' and rs.subject_id = " + txn.quote(
                                              row["id"].get<unsigned int>()));

                                      usersWithRoles.push_back(std::make_shared<types::User>(row, roles));
                                  }
                                  return usersWithRoles;
                              });
}

void UserQueries::updateLastLoggedInUser(const unsigned int userId) {
    Transactions::exec("UserQueries::updateLastLoggedInUser", [&](pqxx::work& txn) {
        txn.exec("UPDATE users SET last_login = NOW() WHERE id = " + txn.quote(userId));
    });
}

void UserQueries::addRefreshToken(const std::shared_ptr<vh::auth::RefreshToken>& token) {
    Transactions::exec("UserQueries::addRefreshToken", [&](pqxx::work& txn) {
        txn.exec("INSERT INTO refresh_tokens (jti, user_id, token_hash, ip_address, user_agent) VALUES (" +
                 txn.quote(token->getJti()) + ", " + txn.quote(token->getUserId()) + ", " +
                 txn.quote(token->getHashedToken()) + ", " + txn.quote(token->getIpAddress()) + ", " +
                 txn.quote(token->getUserAgent()) + ")");
    });
}

void UserQueries::removeRefreshToken(const std::string& jti) {
    Transactions::exec("UserQueries::removeRefreshToken", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM refresh_tokens WHERE jti = " + txn.quote(jti));
    });
}

std::shared_ptr<vh::auth::RefreshToken> UserQueries::getRefreshToken(const std::string& jti) {
    return Transactions::exec("UserQueries::getRefreshToken",
                              [&](pqxx::work& txn) -> std::shared_ptr<vh::auth::RefreshToken> {
                                  pqxx::result res = txn.exec(
                                      "SELECT * FROM refresh_tokens WHERE jti = " + txn.quote(jti));
                                  if (res.empty()) return nullptr; // No token found
                                  return std::make_shared<vh::auth::RefreshToken>(res[0]);
                              });
}

std::vector<std::shared_ptr<vh::auth::RefreshToken> > UserQueries::listRefreshTokens(unsigned int userId) {
    return Transactions::exec("UserQueries::listRefreshTokens",
                              [&](pqxx::work& txn)
                              -> std::vector<std::shared_ptr<vh::auth::RefreshToken> > {
                                  pqxx::result res =
                                      txn.exec("SELECT * FROM refresh_tokens WHERE user_id = " +
                                               txn.quote(userId));
                                  std::vector<std::shared_ptr<vh::auth::RefreshToken> > tokens;
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

void UserQueries::revokeAndPurgeRefreshTokens(unsigned int userId) {
    Transactions::exec("UserQueries::revokeAndPurgeRefreshTokens", [&](pqxx::work& txn) {
        // 1. Revoke the most recent unrevoked token
        txn.exec(R"SQL(
            UPDATE refresh_tokens
            SET revoked = TRUE
            WHERE jti = (
                SELECT jti FROM refresh_tokens
                WHERE user_id = )SQL" +
                 txn.quote(userId) + R"SQL(
                  AND revoked = FALSE
                ORDER BY created_at DESC
                LIMIT 1
            )
        )SQL");

        // 2. Delete tokens older than 7 days (clean out true old shit)
        txn.exec(R"SQL(
            DELETE FROM refresh_tokens
            WHERE user_id = )SQL" +
                 txn.quote(userId) + R"SQL(
              AND created_at < now() - INTERVAL '7 days'
        )SQL");

        // 3. Keep only the 5 most recent tokens < 7 days, delete the rest
        txn.exec(R"SQL(
            DELETE FROM refresh_tokens
            WHERE user_id = )SQL" +
                 txn.quote(userId) + R"SQL(
              AND created_at >= now() - INTERVAL '7 days'
              AND jti NOT IN (
                  SELECT jti FROM refresh_tokens
                  WHERE user_id = )SQL" +
                 txn.quote(userId) + R"SQL(
                    AND created_at >= now() - INTERVAL '7 days'
                  ORDER BY created_at DESC
                  LIMIT 5
              )
        )SQL");
    });
}

} // namespace vh::database
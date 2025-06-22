#include "database/Queries/UserQueries.hpp"
#include "types/db/User.hpp"
#include "auth/RefreshToken.hpp"
#include "database/Transactions.hpp"
#include "types/db/Role.hpp"

namespace vh::database {
std::shared_ptr<types::User> UserQueries::getUserByEmail(const std::string& email) {
    return Transactions::exec("UserQueries::getUserByEmail",
                              [&](pqxx::work& txn) -> std::shared_ptr<vh::types::User> {
                                  const pqxx::result res = txn.exec(R"(SELECT * FROM users WHERE email = )" +
                                                              txn.quote(email));

                                  if (res.empty()) throw std::runtime_error("User not found: " + email);
                                  return std::make_shared<vh::types::User>(res[0]);
                              });
}

std::shared_ptr<types::User> UserQueries::getUserById(const unsigned int id) {
    return Transactions::exec("UserQueries::getUserById",
                              [&](pqxx::work& txn) -> std::shared_ptr<vh::types::User> {
                                  const pqxx::result res = txn.exec(R"(SELECT * FROM users WHERE id = )" + txn.quote(id));

                                  if (res.empty()) throw std::runtime_error("User not found with ID: " + std::to_string(id));
                                  return std::make_shared<vh::types::User>(res[0]);
                              });
}

void UserQueries::createUser(const std::shared_ptr<types::User>& user, const std::string& role) {
    Transactions::exec("UserQueries::createUser", [&](pqxx::work& txn) {
        const auto userId = txn.exec("INSERT INTO users (name, email, password_hash, is_active) VALUES (" +
                 txn.quote(user->name) + ", " + txn.quote(user->email) + ", " + txn.quote(user->password_hash)
                 + ", " + txn.quote(user->is_active) + ") RETURNING id").one_row()[0].as<unsigned int>();

        const auto role_id = txn.exec("SELECT id FROM roles WHERE name = "
            + txn.quote(role)).one_row()[0].as<unsigned int>();

        txn.exec("INSERT INTO user_roles (user_id, role_id) VALUES (" + txn.quote(userId) + ", " +
                 txn.quote(role_id) + ")");
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
    return Transactions::exec("UserQueries::listUsers",
                              [&](pqxx::work& txn) -> std::vector<std::shared_ptr<types::User> > {
                                  const pqxx::result res = txn.exec("SELECT * FROM users");
                                  std::vector<std::shared_ptr<types::User> > users;
                                  for (const auto& row : res) users.push_back(std::make_shared<types::User>(row));
                                  return users;
                              });
}

std::vector<std::pair<std::shared_ptr<types::User>, std::string>> UserQueries::listUsersWithRoles() {
    return Transactions::exec("UserQueries::listUsersWithRoles",
                              [&](pqxx::work& txn) -> std::vector<std::pair<std::shared_ptr<types::User>, std::string> > {
                                  const pqxx::result res = txn.exec(R"(SELECT u.*, r.name as role_name
                                                                FROM users u
                                                                JOIN user_roles ur ON u.id = ur.user_id
                                                                JOIN roles r ON ur.role_id = r.id)");
                                  std::vector<std::pair<std::shared_ptr<types::User>, std::string>> usersWithRoles;
                                  for (const auto& row : res) {
                                      usersWithRoles.emplace_back(std::make_shared<types::User>(row),
                                          types::db_role_str_to_cli_str(row["role_name"].as<std::string>()));
                                  }
                                  return usersWithRoles;
                              });
}

std::pair<std::shared_ptr<types::User>, std::string> UserQueries::getUserWithRole(unsigned int userId) {
    return Transactions::exec("UserQueries::getUserWithRole",
                              [&](pqxx::work& txn) -> std::pair<std::shared_ptr<types::User>, std::string> {
                                  const pqxx::result res = txn.exec(R"(SELECT u.*, r.*
                                                                FROM users u
                                                                JOIN user_roles ur ON u.id = ur.user_id
                                                                JOIN roles r ON ur.role_id = r.id
                                                                WHERE u.id = )" + txn.quote(userId));
                                  if (res.empty()) throw std::runtime_error("User not found with ID: " + std::to_string(userId));
                                  const types::User user(res[0]);
                                  const types::Role role(res[0]);
                                  return {std::make_shared<types::User>(user), types::to_string(role.name)};
                              });
}

types::Role UserQueries::getUserRole(const unsigned int userId) {
    return Transactions::exec("UserQueries::getUserRole", [&](pqxx::work& txn) -> types::Role {
        const pqxx::result res = txn.exec("SELECT r.* FROM roles r "
                                    "JOIN user_roles ur ON r.id = ur.role_id "
                                    "WHERE ur.user_id = " +
                                    txn.quote(userId));
        if (res.empty()) throw std::runtime_error("Role not found for user ID: " + std::to_string(userId));
        return types::Role(res[0]);
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

void UserQueries::revokeAllRefreshTokens(unsigned int userId) {
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

std::shared_ptr<vh::types::User> UserQueries::getUserByRefreshToken(const std::string& jti) {
    return Transactions::exec("UserQueries::getUserByRefreshToken",
                              [&](pqxx::work& txn) -> std::shared_ptr<vh::types::User> {
                                  pqxx::result res =
                                      txn.exec("SELECT u.* FROM users u "
                                               "JOIN refresh_tokens rt ON u.id = rt.user_id "
                                               "WHERE rt.jti = " +
                                               txn.quote(jti));
                                  if (res.empty()) return nullptr; // No user found
                                  return std::make_shared<vh::types::User>(res[0]);
                              });
}
} // namespace vh::database
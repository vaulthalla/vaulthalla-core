#include "database/Queries/UserQueries.hpp"
#include "database/Transactions.hpp"
#include "types/User.hpp"

namespace vh::database {
    std::shared_ptr<vh::types::User> UserQueries::getUserByEmail(const std::string& email) {
        return vh::database::Transactions::exec("UserQueries::getUserByEmail", [&](pqxx::work& txn) -> std::shared_ptr<vh::types::User> {
            pqxx::result res = txn.exec(R"(
    SELECT
        id,
        name,
        email,
        password_hash,
        extract(epoch from created_at)::bigint AS created_at,
        extract(epoch from last_login)::bigint AS last_login,
        is_active
    FROM users
    WHERE email = )" + txn.quote(email));

            if (res.empty()) throw std::runtime_error("User not found: " + email);
            return std::make_shared<vh::types::User>(res[0]);
        });
    }

    void UserQueries::createUser(const std::string& name, const std::string& email, const std::string& password_hash) {
        vh::database::Transactions::exec("UserQueries::createUser", [&](pqxx::work& txn) {
            txn.exec("INSERT INTO users (name, email, password_hash, last_login) VALUES (" +
                     txn.quote(name) + ", " + txn.quote(email) + ", " + txn.quote(password_hash) +
                     ", NOW())");
        });
    }

    bool UserQueries::authenticateUser(const std::string& email, const std::string& password) {
        return vh::database::Transactions::exec("UserQueries::authenticateUser", [&](pqxx::work& txn) -> bool {
            pqxx::result res = txn.exec("SELECT password_hash FROM users WHERE email = " + txn.quote(email));
            if (res.empty()) return false; // User not found
            const std::string& storedHash = res[0][0].as<std::string>();
            return storedHash == password;
        });
    }

    void UserQueries::updateUserPassword(const std::string& email, const std::string& newPassword) {
        vh::database::Transactions::exec("UserQueries::updateUserPassword", [&](pqxx::work& txn) {
            txn.exec("UPDATE users SET password_hash = " + txn.quote(newPassword) +
                     " WHERE email = " + txn.quote(email));
        });
    }

    void UserQueries::deleteUser(const std::string& email) {
        vh::database::Transactions::exec("UserQueries::deleteUser", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM users WHERE email = " + txn.quote(email));
        });
    }
}

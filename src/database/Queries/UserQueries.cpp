#include "database/Queries/UserQueries.hpp"
#include "database/Transactions.hpp"
#include "types/User.hpp"

namespace vh::database {
    vh::types::User UserQueries::getUserByEmail(const std::string& email) {
        return runTransaction("UserQueries::getUserByEmail", [&](pqxx::work& txn) -> vh::types::User {
            pqxx::result res = txn.exec("SELECT * FROM users WHERE email = " + txn.quote(email));
            if (res.empty()) throw std::runtime_error("User not found: " + email);
            return vh::types::User(res[0]);
        });
    }

    void UserQueries::createUser(const std::string& name, const std::string& email, const std::string& password_hash) {
        runTransaction("UserQueries::createUser", [&](pqxx::work& txn) {
            txn.exec("INSERT INTO users (name, email, password_hash, last_login) VALUES (" +
                     txn.quote(name) + ", " + txn.quote(email) + ", " + txn.quote(password_hash) +
                     ", NOW())");
        });
    }

    bool UserQueries::authenticateUser(const std::string& email, const std::string& password) {
        return runTransaction("UserQueries::authenticateUser", [&](pqxx::work& txn) -> bool {
            pqxx::result res = txn.exec("SELECT password_hash FROM users WHERE email = " + txn.quote(email));
            if (res.empty()) return false; // User not found
            const std::string& storedHash = res[0][0].as<std::string>();
            return storedHash == password;
        });
    }

    void UserQueries::updateUserPassword(const std::string& email, const std::string& newPassword) {
        runTransaction("UserQueries::updateUserPassword", [&](pqxx::work& txn) {
            txn.exec("UPDATE users SET password_hash = " + txn.quote(newPassword) +
                     " WHERE email = " + txn.quote(email));
        });
    }

    void UserQueries::deleteUser(const std::string& email) {
        runTransaction("UserQueries::deleteUser", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM users WHERE email = " + txn.quote(email));
        });
    }
}

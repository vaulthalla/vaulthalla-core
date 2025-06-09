#include "database/Queries/UserQueries.hpp"
#include "database/Transactions.hpp"

namespace vh::database {
    void UserQueries::createUser(const std::string& username, const std::string& password) {
        runTransaction("UserQueries::createUser", [&](pqxx::work& txn) {
            txn.exec("INSERT INTO users (username, password) VALUES (" +
                     txn.quote(username) + ", " + txn.quote(password) + ")");
        });
    }

    bool UserQueries::authenticateUser(const std::string& username, const std::string& password) {
        return runTransaction("UserQueries::authenticateUser", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM users WHERE username = " + txn.quote(username) +
                                        " AND password = " + txn.quote(password));
            return !res.empty();
        });
    }

    void UserQueries::updateUserPassword(const std::string& username, const std::string& newPassword) {
        runTransaction("UserQueries::updateUserPassword", [&](pqxx::work& txn) {
            txn.exec("UPDATE users SET password = " + txn.quote(newPassword) +
                     " WHERE username = " + txn.quote(username));
        });
    }

    void UserQueries::deleteUser(const std::string& username) {
        runTransaction("UserQueries::deleteUser", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM users WHERE username = " + txn.quote(username));
        });
    }

    std::string UserQueries::getUserPasswordHash(const std::string& username) const {
        return runTransaction("UserQueries::getUserPasswordHash", [&](pqxx::work& txn) -> std::string {
            pqxx::result res = txn.exec("SELECT password FROM users WHERE username = " + txn.quote(username));
            if (res.empty()) {
                throw std::runtime_error("User not found: " + username);
            }
            return res[0][0].as<std::string>();
        });
    }
}

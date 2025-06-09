#pragma once

#include <string>

namespace vh::database {
    class UserQueries {
    public:
        UserQueries() = default;

        void createUser(const std::string& username, const std::string& password);
        bool authenticateUser(const std::string& username, const std::string& password);
        void updateUserPassword(const std::string& username, const std::string& newPassword);
        void deleteUser(const std::string& username);

        std::string getUserPasswordHash(const std::string& username) const;
    };
}

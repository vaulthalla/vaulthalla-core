#pragma once

#include <string>
#include <memory>

namespace vh::types {
    class User;
}

namespace vh::database {
    class UserQueries {
    public:
        UserQueries() = default;

        [[nodiscard]] static std::shared_ptr<vh::types::User> getUserByEmail(const std::string& email) ;
        static void createUser(const std::string& name, const std::string& email, const std::string& password_hash);
        static bool authenticateUser(const std::string& email, const std::string& password);
        static void updateUserPassword(const std::string& email, const std::string& newPassword);
        static void deleteUser(const std::string& email);
    };
}

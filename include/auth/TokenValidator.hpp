#pragma once

#include <string>

namespace vh::auth {

    class TokenValidator {
    public:
        TokenValidator() = default;
        explicit TokenValidator(std::string  jwtSecret);

        std::string generateToken(const std::string& username);
        bool validateToken(const std::string& token);
        std::string extractUsername(const std::string& token);

    private:
        std::string jwtSecret_;
    };

} // namespace vh::auth

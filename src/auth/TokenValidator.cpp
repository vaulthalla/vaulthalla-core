#include "auth/TokenValidator.hpp"

#include <jwt-cpp/jwt.h>
#include <iostream>
#include <stdexcept>

namespace vh::auth {

    TokenValidator::TokenValidator(const std::string& jwtSecret)
            : jwtSecret_(jwtSecret) {}

    std::string TokenValidator::generateToken(const std::string& username) {
        auto token = jwt::create()
                .set_issuer("vaulthalla")
                .set_type("JWS")
                .set_subject(username)
                .set_issued_at(std::chrono::system_clock::now())
                .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes(60)) // 1 hour tokens
                .sign(jwt::algorithm::hs256{jwtSecret_});

        return token;
    }

    bool TokenValidator::validateToken(const std::string& token) {
        try {
            auto decoded = jwt::decode(token);

            auto verifier = jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{jwtSecret_})
                    .with_issuer("vaulthalla");

            verifier.verify(decoded);

            return true;
        } catch (const std::exception& e) {
            std::cerr << "[TokenValidator] Token validation failed: " << e.what() << "\n";
            return false;
        }
    }

    std::string TokenValidator::extractUsername(const std::string& token) {
        try {
            auto decoded = jwt::decode(token);
            return decoded.get_subject(); // subject = username
        } catch (const std::exception& e) {
            std::cerr << "[TokenValidator] Failed to extract username: " << e.what() << "\n";
            throw std::runtime_error("Invalid token");
        }
    }

} // namespace vh::auth

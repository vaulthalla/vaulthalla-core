#pragma once

#include "types/User.hpp"
#include "Token.hpp"
#include "websocket/WebSocketSession.hpp"

#include <sodium.h>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>
#include <iostream>
#include <libenvpp/env.hpp>

namespace vh::auth {

    class Client {
    public:
        Client() = default;

        explicit Client(const std::shared_ptr<vh::types::User>& user,
                        const std::shared_ptr<vh::websocket::WebSocketSession>& session)
            : user_(user),
            token_(std::make_shared<Token>(generateToken(user->email), user_->id)),
            session_(session) {
            auto pre = env::prefix("VAULTHALLA");
            const auto jwt_secret = pre.register_variable<std::string>("JWT_SECRET");
            const auto parsed = pre.parse_and_validate();
            jwt_secret_ = parsed.get_or(jwt_secret, "3ad8b5821c0c813d4f00cac36ff095aee53ea1e63bc612c4440ad8911ea103d1");
        }

        [[nodiscard]] std::shared_ptr<vh::types::User> getUser() const { return user_; }
        [[nodiscard]] std::shared_ptr<Token> getToken() const { return token_; }
        [[nodiscard]] std::shared_ptr<vh::websocket::WebSocketSession> getSession() const { return session_; }
        void setUser(const std::shared_ptr<vh::types::User>& user) { user_ = user; }
        void setToken(const std::shared_ptr<Token>& token) { token_ = token; }

        [[nodiscard]] std::string getUserName() const { return user_ ? user_->name : ""; }
        [[nodiscard]] std::string getEmail() const { return user_ ? user_->email : ""; }
        [[nodiscard]] std::string getRawToken() const { return token_ ? token_->rawToken : ""; }

        [[nodiscard]] bool expiresSoon() const {
            return token_ && token_->expiryTs < std::chrono::system_clock::now() + std::chrono::minutes(5);
        }

        [[nodiscard]] bool isAuthenticated() const {
            return user_ != nullptr && token_ != nullptr && !token_->isExpired() && !token_->revoked;
        }

        void refreshToken() {
            if (user_) token_ = std::make_shared<Token>(generateToken(user_->email), user_->id);
            else std::cerr << "[Client] Cannot refresh token: user is not set.\n";
        }

        void invalidateToken() {
            if (token_) {
                token_->revoke();
                std::cout << "[Client] Token invalidated for user: " << user_->email << "\n";
            }
            else std::cerr << "[Client] Cannot invalidate token: token is not set.\n";
        }

        void closeConnection() {
            if (session_) {
                invalidateToken();
                session_->close();
                std::cout << "[Client] Connection closed for user: " << user_->email << "\n";
            } else {
                std::cerr << "[Client] Cannot close connection: session is not set.\n";
            }
        }

        bool validateToken(const std::string& token) {
            try {
                auto decoded = jwt::decode<jwt::traits::nlohmann_json>(token);

                auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
                        .allow_algorithm(jwt::algorithm::hs256{jwt_secret_})
                        .with_issuer("vaulthalla");

                verifier.verify(decoded);

                return true;
            } catch (const std::exception& e) {
                std::cerr << "[TokenValidator] Token validation failed: " << e.what() << "\n";
                return false;
            }
        }

        void sendControlMessage(const std::string& type, const nlohmann::json& payload) {
            if (!isAuthenticated()) {
                std::cerr << "[Client] Cannot send control message: user is not authenticated.\n";
                return;
            }

            nlohmann::json msg = {
                {"type", type},
                {"user", user_->email},
                {"payload", payload}
            };

            if (session_) session_->send(msg);
            else std::cerr << "[Client] Cannot send control message: session is not set.\n";
        }

    private:
        std::shared_ptr<vh::types::User> user_;
        std::shared_ptr<Token> token_;
        std::shared_ptr<vh::websocket::WebSocketSession> session_;
        std::string jwt_secret_;

        std::string generateToken(const std::string& email) {
            auto token = jwt::create<jwt::traits::nlohmann_json>()
                    .set_issuer("vaulthalla")
                    .set_type("JWS")
                    .set_subject(email)
                    .set_issued_at(std::chrono::system_clock::now())
                    .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes(60)) // 1 hour tokens
                    .sign(jwt::algorithm::hs256{jwt_secret_});

            return token;
        }
    };

} // namespace vh::auth

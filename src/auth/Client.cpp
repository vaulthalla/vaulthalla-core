#include "auth/Client.hpp"

namespace vh::auth {

    Client::Client() = default;

    Client::Client(const std::shared_ptr<vh::websocket::WebSocketSession>& session,
                   const std::shared_ptr<vh::types::User>& user)
            : user_(user), session_(session) {}

    std::shared_ptr<vh::types::User> Client::getUser() const { return user_; }
    std::shared_ptr<Token> Client::getToken() const { return token_; }
    std::shared_ptr<vh::websocket::WebSocketSession> Client::getSession() const { return session_; }

    void Client::setUser(const std::shared_ptr<vh::types::User>& user) {
        user_ = user;
        token_ = std::make_shared<Token>(generateToken(user->email), user->id);
        session_->setAuthenticatedUser(user_);
    }
    void Client::setToken(const std::shared_ptr<Token>& token) { token_ = token; }

    std::string Client::getUserName() const { return user_ ? user_->name : ""; }
    std::string Client::getEmail() const { return user_ ? user_->email : ""; }
    std::string Client::getRawToken() const { return token_ ? token_->rawToken : ""; }

    bool Client::expiresSoon() const {
        return token_ && token_->expiryTs < std::chrono::system_clock::now() + std::chrono::minutes(5);
    }

    bool Client::isAuthenticated() const {
        return user_ != nullptr && token_ != nullptr && !token_->isExpired() && !token_->revoked;
    }

    void Client::refreshToken() {
        if (user_) token_ = std::make_shared<Token>(generateToken(user_->email), user_->id);
        else std::cerr << "[Client] Cannot refresh token: user is not set.\n";
    }

    void Client::invalidateToken() {
        if (token_) {
            token_->revoke();
            std::cout << "[Client] Token invalidated for user: " << user_->email << "\n";
        } else {
            std::cerr << "[Client] Cannot invalidate token: token is not set.\n";
        }
    }

    void Client::closeConnection() {
        if (session_) {
            invalidateToken();
            session_->close();
            std::cout << "[Client] Connection closed for user: " << user_->email << "\n";
        } else {
            std::cerr << "[Client] Cannot close connection: session is not set.\n";
        }
    }

    bool Client::validateToken(const std::string& token) {
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

    void Client::sendControlMessage(const std::string& type, const nlohmann::json& payload) {
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

    std::string Client::generateToken(const std::string& email) {
        auto token = jwt::create<jwt::traits::nlohmann_json>()
                .set_issuer("vaulthalla")
                .set_type("JWS")
                .set_subject(email)
                .set_issued_at(std::chrono::system_clock::now())
                .set_expires_at(std::chrono::system_clock::now() + std::chrono::minutes(60))
                .sign(jwt::algorithm::hs256{jwt_secret_});

        return token;
    }

} // namespace vh::auth

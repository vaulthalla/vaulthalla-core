#pragma once

#include <memory>
#include <optional>
#include <chrono>
#include <string>

namespace vh::protocols::ws { class Session; }

namespace vh::auth::session {

struct RefreshTokenClaims {
    std::string jti;
    std::string subject;
    std::chrono::system_clock::time_point expiresAt;
};

struct Validator {
    static bool validateSession(const std::shared_ptr<protocols::ws::Session>& session);
    static bool validateAccessToken(const std::shared_ptr<protocols::ws::Session>& session, const std::string& accessToken);
    static bool hasUsableAccessToken(const std::shared_ptr<protocols::ws::Session>& session);
    static bool hasUsableRefreshToken(const std::shared_ptr<protocols::ws::Session>& session);
    static std::optional<RefreshTokenClaims> decodeRefreshToken(const std::string& refreshToken);
};

}

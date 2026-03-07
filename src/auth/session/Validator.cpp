#include "auth/session/Validator.hpp"
#include "auth/model/RefreshToken.hpp"
#include "auth/model/TokenPair.hpp"
#include "protocols/ws/Session.hpp"
#include "log/Registry.hpp"
#include "crypto/secrets/Manager.hpp"

#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>

using namespace vh::auth::session;

bool Validator::validateAccessToken(const std::shared_ptr<protocols::ws::Session>& session, const std::string& accessToken) {
    try {
        if (!hasUsableAccessToken(session)) {
            log::Registry::ws()->debug("[Client] No valid access token found for session");
            return false;
        }

        const auto decoded = jwt::decode<jwt::traits::nlohmann_json>(accessToken);

        const auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
            .allow_algorithm(jwt::algorithm::hs256{crypto::secrets::Manager().jwtSecret()})
            .with_issuer("vaulthalla");

        verifier.verify(decoded);

        return true;
    } catch (const std::exception& e) {
        log::Registry::ws()->debug("[Client] Token validation failed: {}", e.what());
        return false;
    }
}

bool Validator::validateSession(const std::shared_ptr<protocols::ws::Session>& session) {
    return session->user && hasUsableAccessToken(session) && hasUsableRefreshToken(session);
}

bool Validator::hasUsableAccessToken(const std::shared_ptr<protocols::ws::Session>& session) {
    return session->tokens->accessToken && session->tokens->accessToken->isValid();
}

bool Validator::hasUsableRefreshToken(const std::shared_ptr<protocols::ws::Session>& session) {
    return session->tokens->refreshToken && session->tokens->refreshToken->isValid();
}

std::optional<RefreshTokenClaims> Validator::decodeRefreshToken(const std::string& refreshToken) {
    try {
        const auto decoded = jwt::decode<jwt::traits::nlohmann_json>(refreshToken);

        const auto verifier = jwt::verify<jwt::traits::nlohmann_json>()
            .allow_algorithm(jwt::algorithm::hs256{crypto::secrets::Manager().jwtSecret()})
            .with_issuer("Vaulthalla");

        verifier.verify(decoded);

        return RefreshTokenClaims {
            .jti = decoded.get_id(),
            .subject = decoded.get_subject(),
            .expiresAt = decoded.get_expires_at()
        };
    } catch (const std::exception& e) {
        log::Registry::auth()->debug("[AuthManager] Failed to decode refresh token: {}", e.what());
        return std::nullopt;
    }
}

#include "auth/session/Resolver.hpp"
#include "auth/session/Validator.hpp"
#include "auth/session/Manager.hpp"
#include "crypto/secrets/Manager.hpp"
#include "db/query/identities/User.hpp"
#include "protocols/ws/Session.hpp"
#include "auth/model/TokenPair.hpp"
#include "auth/model/RefreshToken.hpp"
#include "identities/model/User.hpp"
#include "runtime/Deps.hpp"
#include "crypto/util/hash.hpp"
#include "log/Registry.hpp"

#include <jwt-cpp/jwt.h>

namespace vh::auth::session {

static void checkForDangerousDiversion(const std::shared_ptr<model::RefreshToken>& incomingToken, const std::shared_ptr<model::RefreshToken>& storedToken) {
    if (incomingToken->dangerousDivergence(storedToken)) {
        const auto msg = fmt::format("[session::Resolver] Dangerous divergence detected, raw tokens match but token details differ for JTI: {}, stored token: {}, incoming token: {}",
            incomingToken->jti,
            storedToken->rawToken,
            incomingToken->rawToken);
        log::Registry::ws()->warn(msg);
        log::Registry::audit()->warn(msg);
        throw std::runtime_error("Potential token tampering detected");
    }
}

static void handlePriorSession(const std::shared_ptr<protocols::ws::Session>& session, const std::shared_ptr<protocols::ws::Session>& priorSession) {
    if (!Validator::validateSession(priorSession)) throw std::runtime_error("Prior session is not authenticated");
    log::Registry::auth()->debug("[session::Resolver] Rehydrating existing session for JTI: {}", priorSession->tokens->refreshToken->jti);
    if (priorSession->tokens->refreshToken != session->tokens->refreshToken) {
        checkForDangerousDiversion(session->tokens->refreshToken, priorSession->tokens->refreshToken);
        runtime::Deps::get().sessionManager->invalidateSession(priorSession);
        log::Registry::auth()->debug("[session::Resolver] Refresh token mismatch for JTI: {}", priorSession->tokens->refreshToken->jti);
        throw std::runtime_error("Refresh token mismatch");
    }
}

static void handleMissingClaims(const std::optional<RefreshTokenClaims>& claims) {
    if (!claims) throw std::runtime_error("Invalid refresh token: unable to decode claims");
    if (claims->expiresAt < std::chrono::system_clock::now()) throw std::runtime_error("Refresh token has expired");
    if (claims->jti.empty()) throw std::runtime_error("Missing JTI in refresh token");
}

void Resolver::resolveFromRefreshToken(const std::shared_ptr<protocols::ws::Session>& session) {
    const auto claims = Validator::decodeRefreshToken(session->tokens->refreshToken->rawToken);
    handleMissingClaims(claims);

    if (const auto priorSession = runtime::Deps::get().sessionManager->getSession(claims->jti))
        handlePriorSession(session, priorSession);

    // detect potential token reuse or tampering by comparing the incoming token with the stored token details
    if (const auto storedToken = db::query::identities::User::getRefreshToken(claims->jti)) {
        checkForDangerousDiversion(session->tokens->refreshToken, storedToken);

        if (!crypto::hash::verifyPassword(session->tokens->refreshToken->rawToken, storedToken->hashedToken))
            throw std::runtime_error(
                "Refresh token hash mismatch");
    }

    if (const auto user = db::query::identities::User::getUserByRefreshToken(claims->jti)) {
        if (user->id != session->tokens->refreshToken->userId) {
            runtime::Deps::get().sessionManager->invalidateSession(session);
            log::Registry::auth()->debug("[session::Resolver] User ID mismatch for JTI: {}, token user ID: {}, expected user ID: {}",
                claims->jti, session->tokens->refreshToken->userId, user->id);
            throw std::runtime_error("User ID mismatch for refresh token");
        }
    } else {
        runtime::Deps::get().sessionManager->invalidateSession(session);
        log::Registry::auth()->debug("[session::Resolver] No user found for JTI: {}, token user ID: {}",
            claims->jti, session->tokens->refreshToken->userId);
        throw std::runtime_error("No user found for refresh token");
    }
}

}

#include "auth/session/Issuer.hpp"
#include "auth/model/RefreshToken.hpp"
#include "config/Registry.hpp"
#include "protocols/ws/Session.hpp"
#include "runtime/Deps.hpp"
#include "crypto/util/hash.hpp"
#include "crypto/secrets/Manager.hpp"

#include <chrono>
#include <uuid/uuid.h>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/defaults.h>

using namespace vh::auth::session;
using namespace vh::protocols::ws;

static std::string generateUUID() {
    uuid_t uuid;
    char uuidStr[37];
    uuid_generate(uuid);
    uuid_unparse(uuid, uuidStr);
    return {uuidStr};
}

std::shared_ptr<vh::auth::model::RefreshToken> Issuer::refreshToken(const std::shared_ptr<Session>& session) {
    const auto now = std::chrono::system_clock::now();
    const auto exp = now + std::chrono::days(config::Registry::get().auth.refresh_token_expiry_days);
    const std::string jti = generateUUID();

    const std::string token =
        jwt::create<jwt::traits::nlohmann_json>()
        .set_issuer("Vaulthalla")
        .set_subject(session->ipAddress + ":" + session->userAgent + ":" + session->uuid)
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_id(jti)
        .sign(jwt::algorithm::hs256{runtime::Deps::get().secretsManager->jwtSecret()});

    return model::RefreshToken::fromIssuedToken(
        jti,
        token,
        crypto::hash::password(token),
        0,
        session->userAgent,
        session->ipAddress
        );
}


#include "auth/model/RefreshToken.hpp"
#include "db/query/auth/RefreshToken.hpp"
#include "db/encoding/timestamp.hpp"
#include "protocols/ws/Session.hpp"
#include "auth/model/TokenPair.hpp"

#include <chrono>
#include <pqxx/row>
#include <utility>

using namespace vh::auth::model;

using namespace vh::db::encoding;

RefreshToken::RefreshToken(std::string jti,
                           std::string rawToken,
                           std::string hashedToken,
                           const uint32_t userId,
                           std::string userAgent,
                           std::string ipAddress)
    : Token(std::move(rawToken), userId),
      jti(std::move(jti)),
      hashedToken(std::move(hashedToken)),
      userAgent(std::move(userAgent)),
      ipAddress(std::move(ipAddress)),
      createdAt(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())),
      lastUsed(createdAt) {
    expiresAt = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now() + std::chrono::hours(24 * 7));
}

RefreshToken::RefreshToken(std::string rawToken) : Token(std::move(rawToken)) {}

RefreshToken::RefreshToken(const pqxx::row& row)
    : Token("", row["user_id"].as<uint32_t>()),
      jti(row["jti"].as<std::string>()),
      hashedToken(row["token_hash"].as<std::string>()),
      userAgent(row["user_agent"].as<std::string>()),
      ipAddress(row["ip_address"].as<std::string>()),
      createdAt(parsePostgresTimestamp(row["created_at"].as<std::string>())),
      lastUsed(parsePostgresTimestamp(row["last_used"].as<std::string>())) {
    rawToken = !row["token"].is_null() ? row["token"].as<std::string>() : "";
    expiresAt = parsePostgresTimestamp(row["expires_at"].as<std::string>());
    revoked = row["revoked"].as<bool>();
}

bool RefreshToken::isValid() const { return !hashedToken.empty() && !revoked && !isExpired(); }

bool RefreshToken::dangerousDivergence(const std::shared_ptr<RefreshToken>& other) const {
    return hashedToken == other->hashedToken && (
        jti != other->jti || userAgent != other->userAgent ||
        ipAddress != other->ipAddress || createdAt != other->createdAt || expiresAt != other->expiresAt ||
        lastUsed != other->lastUsed || revoked != other->revoked
    );
}

void RefreshToken::hardInvalidate() {
    revoke();
    db::query::auth::RefreshToken::revokeRefreshToken(jti);
    hashedToken.clear();
    rawToken.clear();
}

std::shared_ptr<RefreshToken> RefreshToken::fromIssuedToken(std::string jti, std::string rawToken,
                                                            std::string hashedToken, std::uint32_t userId,
                                                            std::string userAgent, std::string ipAddress) {
    return std::make_shared<RefreshToken>(std::move(jti), std::move(rawToken), std::move(hashedToken), userId,
                                          std::move(userAgent), std::move(ipAddress));
}

void RefreshToken::addToSession(const std::shared_ptr<protocols::ws::Session>& session, std::string token) {
    session->tokens->refreshToken = std::make_shared<RefreshToken>(std::move(token));
}

bool vh::auth::model::operator==(const std::shared_ptr<RefreshToken>& lhs, const std::shared_ptr<RefreshToken>& rhs) {
    return lhs->userId == rhs->userId && lhs->expiresAt == rhs->expiresAt &&
           lhs->revoked == rhs->revoked && lhs->jti == rhs->jti && lhs->hashedToken == rhs->hashedToken &&
           lhs->userAgent == rhs->userAgent && lhs->ipAddress == rhs->ipAddress &&
           lhs->createdAt == rhs->createdAt && lhs->expiresAt == rhs->expiresAt &&
           lhs->lastUsed == rhs->lastUsed && lhs->revoked == rhs->revoked;
}

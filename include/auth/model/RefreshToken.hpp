#pragma once

#include "database/encoding/timestamp.hpp"
#include <pqxx/row>
#include <string>
#include <utility>

using namespace vh::database::encoding;

namespace vh::auth::model {

class RefreshToken {
  public:
    RefreshToken(std::string jti, std::string hashedToken, unsigned int userId, std::string userAgent,
                 std::string ipAddress)
        : jti_(std::move(jti)), hashedToken_(std::move(hashedToken)), userAgent_(std::move(userAgent)),
          ipAddress_(std::move(ipAddress)), userId_(userId), // 7 days
          expiresAt_(
              std::chrono::system_clock::to_time_t(std::chrono::system_clock::now() + std::chrono::hours(24 * 7))),
          createdAt_(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())),
          lastUsed_(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())), revoked_(false) {}

    explicit RefreshToken(const pqxx::row& row)
        : jti_(row["jti"].as<std::string>()), hashedToken_(row["token_hash"].as<std::string>()),
          userAgent_(row["user_agent"].as<std::string>()), ipAddress_(row["ip_address"].as<std::string>()),
          userId_(row["user_id"].as<unsigned int>()),
          expiresAt_(parsePostgresTimestamp(row["expires_at"].as<std::string>())),
          createdAt_(parsePostgresTimestamp(row["created_at"].as<std::string>())),
          lastUsed_(parsePostgresTimestamp(row["last_used"].as<std::string>())),
          revoked_(row["revoked"].as<bool>()) {}

    [[nodiscard]] const std::string& getJti() const { return jti_; }
    [[nodiscard]] const std::string& getHashedToken() const { return hashedToken_; }
    [[nodiscard]] unsigned int getUserId() const { return userId_; }
    [[nodiscard]] std::time_t getExpiresAt() const { return expiresAt_; }
    [[nodiscard]] std::time_t getCreatedAt() const { return createdAt_; }
    [[nodiscard]] std::time_t getLastUsed() const { return lastUsed_; }
    [[nodiscard]] bool isRevoked() const { return revoked_; }
    [[nodiscard]] const std::string& getUserAgent() const { return userAgent_; }
    [[nodiscard]] const std::string& getIpAddress() const { return ipAddress_; }

    void setRevoked(bool revoked) { revoked_ = revoked; }
    void setUserId(unsigned int userId) { userId_ = userId; }
    void setUserAgent(const std::string& userAgent) { userAgent_ = userAgent; }
    void setIpAddress(const std::string& ipAddress) { ipAddress_ = ipAddress; }

  private:
    const std::string jti_, hashedToken_;
    std::string userAgent_, ipAddress_;
    unsigned int userId_;
    std::time_t expiresAt_, createdAt_, lastUsed_;
    bool revoked_;
};

}

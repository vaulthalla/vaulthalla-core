#pragma once

#include <chrono>
#include <string>
#include <utility>

namespace vh::auth {

struct Token {
    Token(std::string token, unsigned short userId)
        : rawToken(std::move(token)), userId(userId),
          expiryTs(std::chrono::system_clock::now() + std::chrono::hours(1)) {}

    std::string rawToken;
    unsigned short userId;
    std::chrono::system_clock::time_point expiryTs;
    bool revoked = false;

    [[nodiscard]] bool isExpired() const { return std::chrono::system_clock::now() >= expiryTs; }
    [[nodiscard]] bool isValid() const { return !isExpired() && !revoked; }

    [[nodiscard]] long long getTimeLeft() const {
        return std::chrono::duration_cast<std::chrono::seconds>(expiryTs - std::chrono::system_clock::now()).count();
    }

    void revoke() { revoked = true; }
};

} // namespace vh::auth

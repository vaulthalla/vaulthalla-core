#pragma once
#include <memory>

namespace vh::auth::model {

struct Token;
struct RefreshToken;

struct TokenPair {
    std::shared_ptr<Token> accessToken;
    std::shared_ptr<RefreshToken> refreshToken;

    void revoke() const;
    void invalidate() const;
    void destroy();
};

}

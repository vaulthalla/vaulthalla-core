#pragma once

#include <memory>
#include <string>
#include <vector>

namespace vh::auth::model { struct RefreshToken; }
namespace vh::identities::model { struct User; }

namespace vh::db::query::auth {

struct RefreshToken {
    static void setRefreshToken(const std::shared_ptr<vh::auth::model::RefreshToken>& token);
    static void removeRefreshToken(const std::string& jti);

    static std::shared_ptr<vh::auth::model::RefreshToken> getRefreshToken(const std::string& jti);
    static std::vector<std::shared_ptr<vh::auth::model::RefreshToken>> listRefreshTokens(unsigned int userId);

    static void touchRefreshToken(const std::string& jti);
    static void revokeRefreshToken(const std::string& jti);
    static void revokeAllRefreshTokens(unsigned int userId);

    static void purgeExpiredRefreshTokens(unsigned int userId);
    static void purgeOldRevokedRefreshTokens();
    static void revokeAndPurgeRefreshTokens(unsigned int userId);

    static std::shared_ptr<identities::model::User> getUserByRefreshToken(const std::string& jti);
};

}

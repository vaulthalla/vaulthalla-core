#include "auth/model/TokenPair.hpp"
#include "auth/model/RefreshToken.hpp"

using namespace vh::auth::model;

void TokenPair::invalidate() {
    accessToken->revoke();
    refreshToken->invalidate();
    accessToken = nullptr;
    refreshToken = nullptr;
}

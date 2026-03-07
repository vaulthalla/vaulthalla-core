#pragma once

#include <memory>

namespace vh::auth::model { struct RefreshToken; }
namespace vh::protocols::ws { class Session; }

namespace vh::auth::session {

struct Issuer {
    static std::shared_ptr<model::RefreshToken> refreshToken(const std::shared_ptr<protocols::ws::Session>& session);
};

}

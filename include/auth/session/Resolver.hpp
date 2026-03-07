#pragma once

#include <memory>

namespace vh::protocols::ws { class Session; }

namespace vh::auth::session {

class Manager;

class Resolver {
public:
    static void resolveFromRefreshToken(const std::shared_ptr<protocols::ws::Session>& session);
};

}

#pragma once

#include "session/Manager.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <jwt-cpp/jwt.h>
#include <jwt-cpp/traits/nlohmann-json/traits.h>

namespace vh::identities::model { struct User; }
namespace vh::protocols::ws { class Session; }
namespace vh::storage { class Manager; }

namespace vh::auth {

namespace model { struct RefreshToken; }
namespace session { class Manager; }

class Manager {
public:
    std::shared_ptr<session::Manager> sessionManager;

    explicit Manager(const std::shared_ptr<storage::Manager>& storageManager = nullptr);

    void rehydrateOrCreateClient(const std::shared_ptr<protocols::ws::Session>& session) const;

    std::shared_ptr<protocols::ws::Session> registerUser(std::shared_ptr<identities::model::User> user,
                                         const std::string& password,
                                         const std::shared_ptr<protocols::ws::Session>& session);

    std::shared_ptr<protocols::ws::Session> loginUser(const std::string& name, const std::string& password,
                                      const std::shared_ptr<protocols::ws::Session>& session);

    void updateUser(const std::shared_ptr<identities::model::User>& user);

    void changePassword(const std::string& name, const std::string& oldPassword, const std::string& newPassword);

    std::shared_ptr<identities::model::User> findUser(const std::string& name);

    void validateSession(const std::shared_ptr<protocols::ws::Session>& session);

    std::shared_ptr<model::RefreshToken>
    createRefreshToken(const std::shared_ptr<protocols::ws::Session>& session) const;

private:
    std::unordered_map<std::string, std::shared_ptr<identities::model::User>> users_;
    std::shared_ptr<storage::Manager> storageManager_;
    const std::string jwt_secret_ = crypto::secrets::Manager().jwtSecret();
};

}

#pragma once

#include "security/PermissionManager.hpp"
#include "auth/User.hpp"

#include <memory>
#include <string>

namespace vh::security {

    class AccessControl {
    public:
        explicit AccessControl(std::shared_ptr<PermissionManager> permissionManager);

        void enforcePermission(std::shared_ptr<vh::auth::User> user,
                               const std::string& mountName,
                               const std::string& path,
                               const std::string& requiredPermission);

        [[nodiscard]] std::shared_ptr<PermissionManager> permissionManager() const;

    private:
        std::shared_ptr<PermissionManager> permissionManager_;
    };

} // namespace vh::security

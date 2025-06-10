#include "security/AccessControl.hpp"

#include <stdexcept>
#include <iostream>

namespace vh::security {

    AccessControl::AccessControl()
            : permissionManager_(std::make_shared<PermissionManager>()) {}

    std::shared_ptr<PermissionManager> AccessControl::permissionManager() const { return permissionManager_; }

    void AccessControl::enforcePermission(const std::shared_ptr<vh::types::User>& user,
                                          const std::string& mountName,
                                          const std::string& path,
                                          const std::string& requiredPermission) {
        if (!user) {
            throw std::runtime_error("Unauthorized: no user bound to session");
        }

        const std::string& email = user->email;

        if (!permissionManager_->hasPermission(email, mountName, path, requiredPermission)) {
            std::cerr << "[AccessControl] Permission denied: user " << email
                      << " lacks " << requiredPermission << " on "
                      << mountName << ":" << path << "\n";

            throw std::runtime_error("Permission denied: " + requiredPermission + " on " + path);
        }

        std::cout << "[AccessControl] Permission granted: user " << email
                  << " " << requiredPermission << " on "
                  << mountName << ":" << path << "\n";
    }

} // namespace vh::security

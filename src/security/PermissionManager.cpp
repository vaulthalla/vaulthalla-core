#include "security/PermissionManager.hpp"

#include <algorithm>
#include <iostream>

namespace vh::security {

    void PermissionManager::addPermission(const Permission& permission) {
        std::lock_guard<std::mutex> lock(permissionsMutex_);
        permissions_.push_back(permission);

        std::cout << "[PermissionManager] Added permission: "
                  << permission.username << " " << permission.permissionType
                  << " " << permission.mountName << ":" << permission.path << "\n";
    }

    void PermissionManager::removePermission(const Permission& permission) {
        std::lock_guard<std::mutex> lock(permissionsMutex_);

        permissions_.erase(std::remove_if(permissions_.begin(), permissions_.end(),
                                          [&](const Permission& p) {
                                              return p.username == permission.username &&
                                                     p.mountName == permission.mountName &&
                                                     p.path == permission.path &&
                                                     p.permissionType == permission.permissionType;
                                          }), permissions_.end());

        std::cout << "[PermissionManager] Removed permission: "
                  << permission.username << " " << permission.permissionType
                  << " " << permission.mountName << ":" << permission.path << "\n";
    }

    bool PermissionManager::hasPermission(const std::string& username,
                                          const std::string& mountName,
                                          const std::string& path,
                                          const std::string& permissionType) {
        std::lock_guard<std::mutex> lock(permissionsMutex_);

        for (const auto& p : permissions_) {
            if (p.username == username &&
                p.mountName == mountName &&
                permissionType == p.permissionType) {

                // Path prefix matching → allow wildcard-like behavior
                if (path.rfind(p.path, 0) == 0) { // path starts with p.path
                    return true;
                }
            }
        }

        return false;
    }

    std::vector<Permission> PermissionManager::listPermissionsForUser(const std::string& username) {
        std::lock_guard<std::mutex> lock(permissionsMutex_);

        std::vector<Permission> result;
        for (const auto& p : permissions_) {
            if (p.username == username) {
                result.push_back(p);
            }
        }

        return result;
    }

} // namespace vh::security

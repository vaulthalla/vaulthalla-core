#pragma once

#include "security/Permission.hpp"

#include <string>
#include <vector>
#include <mutex>

namespace vh::security {

    class PermissionManager {
    public:
        void addPermission(const Permission& permission);
        void removePermission(const Permission& permission);

        bool hasPermission(const std::string& username,
                           const std::string& mountName,
                           const std::string& path,
                           const std::string& permissionType);

        std::vector<Permission> listPermissionsForUser(const std::string& username);

    private:
        std::vector<Permission> permissions_;
        std::mutex permissionsMutex_;
    };

} // namespace vh::security

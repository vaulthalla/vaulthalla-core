#pragma once

#include <string>

namespace vh::security {

    struct Permission {
        std::string username;
        std::string mountName;
        std::string path;           // Path prefix this permission applies to
        std::string permissionType; // e.g. "READ", "WRITE"

        Permission(const std::string& username_,
                   const std::string& mountName_,
                   const std::string& path_,
                   const std::string& permissionType_)
                : username(username_),
                  mountName(mountName_),
                  path(path_),
                  permissionType(permissionType_) {}
    };

} // namespace vh::security

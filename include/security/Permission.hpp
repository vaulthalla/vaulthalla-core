#pragma once

#include <string>
#include <utility>

namespace vh::security {

    struct Permission {
        std::string username;
        std::string mountName;
        std::string path;           // Path prefix this permission applies to
        std::string permissionType; // e.g. "READ", "WRITE"

        Permission(std::string  username_,
                   std::string  mountName_,
                   std::string  path_,
                   std::string  permissionType_)
                : username(std::move(username_)),
                  mountName(std::move(mountName_)),
                  path(std::move(path_)),
                  permissionType(std::move(permissionType_)) {}
    };

} // namespace vh::security

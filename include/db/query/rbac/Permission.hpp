#pragma once

#include "db/model/ListQueryParams.hpp"

#include <memory>
#include <vector>
#include <string>

namespace vh::rbac::permission { struct Permission; }

namespace vh::db::query::rbac {

class Permission {
    using Perm = vh::rbac::permission::Permission;
    using PermPtr = std::shared_ptr<Perm>;

public:
    static PermPtr getPermission(unsigned int id);
    static PermPtr getPermissionByName(const std::string& name);
    static std::vector<PermPtr> listPermissions();
    static unsigned short countPermissions();
};

}

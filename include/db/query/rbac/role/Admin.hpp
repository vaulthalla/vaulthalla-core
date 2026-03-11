#pragma once

#include "db/model/ListQueryParams.hpp"

#include <memory>
#include <vector>
#include <string>

namespace vh::rbac {
namespace role { struct User; }
namespace permission { struct Admin; }
}

namespace vh::db::query::rbac::role {

class Admin {
    using UserRole = vh::rbac::permission::Admin;
    using UserRolePtr = std::shared_ptr<UserRole>;

public:
    static unsigned int upsert(const UserRolePtr& role);
    static void remove(unsigned int id);
    static void remove(const UserRolePtr& userRole);
    static UserRolePtr get(unsigned int id);
    static std::vector<UserRolePtr> list(model::ListQueryParams&& params = {});
};

}

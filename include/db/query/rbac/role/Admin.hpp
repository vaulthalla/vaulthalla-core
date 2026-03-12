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
    using AdminRole = vh::rbac::permission::Admin;
    using AdminRolePtr = std::shared_ptr<AdminRole>;

public:
    static unsigned int upsert(const AdminRolePtr& role);
    static void remove(unsigned int id);
    static void remove(const AdminRolePtr& userRole);
    static AdminRolePtr get(unsigned int id);
    static std::vector<AdminRolePtr> list(model::ListQueryParams&& params = {});
};

}

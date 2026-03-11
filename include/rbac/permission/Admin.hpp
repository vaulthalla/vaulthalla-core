#pragma once

#include "rbac/permission/admin/Identities.hpp"
#include "rbac/permission/admin/Vaults.hpp"
#include "rbac/permission/admin/Audits.hpp"
#include "rbac/permission/admin/Settings.hpp"

namespace vh::rbac::permission {

struct Admin {
    admin::Identities identities;
    admin::Vaults vaults;
    admin::Audits audits;
    admin::Settings settings;
};

}

#pragma once

#include "rbac/permission/admin/Identities.hpp"
#include "rbac/permission/admin/Vaults.hpp"
#include "rbac/permission/admin/Audits.hpp"
#include "rbac/permission/admin/Settings.hpp"

namespace pqxx { class row; class result; }

namespace vh::rbac::permission {

struct Admin {
    admin::Identities identities;
    admin::Vaults vaults;
    admin::Audits audits;
    admin::Settings settings;

    Admin() = default;
    Admin(const pqxx::row& row, const pqxx::result& vaultGlobalPerms);
};

}

#pragma once

#include "rbac/permission/vault/Keys.hpp"
#include "rbac/permission/vault/Roles.hpp"
#include "rbac/permission/vault/Sync.hpp"
#include "rbac/permission/vault/Filesystem.hpp"

namespace pqxx { class row; }

namespace vh::rbac::permission {

struct Vault {
    vault::Keys keys{};
    vault::Roles roles{};
    vault::Sync sync{};
    vault::Filesystem filesystem{};

    Vault() = default;
    explicit Vault(const pqxx::row& row);
};

}

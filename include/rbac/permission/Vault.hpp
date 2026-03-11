#pragma once

#include "rbac/permission/vault/Keys.hpp"
#include "rbac/permission/vault/Roles.hpp"
#include "rbac/permission/vault/Sync.hpp"
#include "rbac/permission/vault/Files.hpp"
#include "rbac/permission/vault/Directories.hpp"

namespace vh::rbac::permission {

struct Vault {
    vault::Keys keys;
    vault::Roles roles;
    vault::Sync sync;
    vault::Files files;
    vault::Directories directories;
};

}

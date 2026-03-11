#pragma once

#include "rbac/permission/Vault.hpp"

#include <cstdint>

namespace vh::rbac::permission::admin {

struct Vaults {
    Vault self, admin, user;
};

}

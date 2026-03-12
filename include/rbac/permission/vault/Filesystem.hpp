#pragma once

#include "rbac/permission/vault/Files.hpp"
#include "rbac/permission/vault/Directories.hpp"
#include "rbac/permission/Override.hpp"

#include <vector>
#include <memory>

namespace pqxx { class row; }

namespace vh::rbac::permission::vault {

struct Filesystem {
    Files files;
    Directories directories;
    std::vector<std::shared_ptr<Override>> overrides;

    Filesystem() = default;
    explicit Filesystem(const pqxx::row& row);
};

}

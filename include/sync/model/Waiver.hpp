#pragma once

#include <memory>
#include <string>

namespace vh::vault::model { struct APIKey; struct S3Vault; }
namespace vh::identities::model { struct User; }
namespace vh::rbac::model { struct Role; }

namespace vh::sync::model {

struct Waiver {
    unsigned int id{};
    std::shared_ptr<vault::model::S3Vault> vault;
    std::shared_ptr<identities::model::User> user, owner;
    std::shared_ptr<vault::model::APIKey> apiKey;
    std::shared_ptr<rbac::model::Role> overridingRole;
    bool encrypt_upstream{};
    std::string waiver_text;
};

}
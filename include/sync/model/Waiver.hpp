#pragma once

#include <memory>
#include <string>

namespace vh::types {
struct S3Vault;
struct User;
struct Role;

namespace api {
struct APIKey;
}
}

namespace vh::sync::model {

struct Waiver {
    unsigned int id{};
    std::shared_ptr<types::S3Vault> vault;
    std::shared_ptr<types::User> user, owner;
    std::shared_ptr<types::api::APIKey> apiKey;
    std::shared_ptr<types::Role> overridingRole;
    bool encrypt_upstream{};
    std::string waiver_text;
};

}
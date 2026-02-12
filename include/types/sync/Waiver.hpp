#pragma once

#include <memory>
#include <string>

namespace vh::types {

namespace api {
struct APIKey;
}

struct S3Vault;
struct User;
struct Role;

struct Waiver {
    unsigned int id{};
    std::shared_ptr<S3Vault> vault;
    std::shared_ptr<User> user, owner;
    std::shared_ptr<api::APIKey> apiKey;
    std::shared_ptr<Role> overridingRole;
    bool encrypt_upstream{};
    std::string waiver_text;
};

}
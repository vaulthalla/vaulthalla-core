#pragma once

#include <memory>
#include <vector>
#include <string>
#include <functional>
#include <array>

namespace vh::types {
struct User;
struct APIKey;
struct Vault;
struct Role;
struct Group;
}

namespace vh::test::cli {

struct CLITestContext;

struct TestStage {
    std::string name;
    std::function<void(CLITestContext&)> run;
};

static constexpr std::array<TestStage, 3> kDefaultTestStages = {{
    {"phase0: seed baseline", [](CLITestContext& ctx) {}},
    {"phase1: generate entities", [](CLITestContext& ctx) {}},
    {"phase2: run tests", [](CLITestContext& ctx) {}},
}};

struct CLITestContext {
    std::shared_ptr<TestStage> stage;

    std::vector<std::shared_ptr<types::User>> users;
    std::vector<std::shared_ptr<types::APIKey>> api_keys;
    std::vector<std::shared_ptr<types::Vault>> vaults;
    std::vector<std::shared_ptr<types::Role>> userRoles, vaultRoles;
    std::vector<std::shared_ptr<types::Group>> groups;

    std::shared_ptr<types::User> pickRandomUser() const;
    std::shared_ptr<types::Vault> pickVaultOwnedBy(const std::shared_ptr<types::User>& user) const;
    std::shared_ptr<types::UserRole> randomUserRole() const;
    std::shared_ptr<types::VaultRole> randomVaultRole() const;
};

}

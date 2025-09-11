#pragma once

#include "EntityType.hpp"
#include "CLITestContext.hpp"
#include "CommandUsage.hpp"

#include <string>
#include <vector>
#include <memory>
#include <algorithm>
#include <ranges>
#include <stdexcept>

namespace vh::test::cli {

static bool isFieldMatch(const std::string& field, const std::vector<std::string>& aliases) {
    return std::ranges::any_of(aliases, [&field](const std::string& t){ return t == field; });
}

struct UserAliases {
    std::vector<std::string> nameAliases;
    std::vector<std::string> emailAliases;
    std::vector<std::string> roleAliases;

    explicit UserAliases(const std::shared_ptr<CLITestContext>& ctx) {
        const auto cmd = ctx->getCommand(EntityType::USER, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for user update");
        for (const auto& opt : cmd->optional) {
            if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "name" || t == "username"; })) {
                nameAliases.insert(nameAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "email"; })) {
                emailAliases.insert(emailAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "role" || t == "role_id"; })) {
                roleAliases.insert(roleAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "linux_uid" || t == "uid"; })) {
                roleAliases.insert(roleAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            }
        }
    }

    bool isName (const std::string& field) const { return isFieldMatch(field, nameAliases); }
    bool isEmail (const std::string& field) const { return isFieldMatch(field, emailAliases); }
    bool isRole (const std::string& field) const { return isFieldMatch(field, roleAliases); }
    bool isLinuxUID (const std::string& field) const { return isFieldMatch(field, roleAliases); }
};

struct GroupAliases {
    std::vector<std::string> nameAliases;

    explicit GroupAliases(const std::shared_ptr<CLITestContext>& ctx) {
        const auto cmd = ctx->getCommand(EntityType::GROUP, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for group update");
        for (const auto& opt : cmd->optional) {
            if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "name" || t == "group_name"; })) {
                nameAliases.insert(nameAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            }
        }
    }

    bool isName (const std::string& field) const { return isFieldMatch(field, nameAliases); }
};

struct UserRoleAliases {
    std::vector<std::string> nameAliases, permAliases, descAliases;

    explicit UserRoleAliases(const std::shared_ptr<CLITestContext>& ctx) {
        const auto cmd = ctx->getCommand(EntityType::USER_ROLE, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for user role update");
        for (const auto& opt : cmd->optional) {
            if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "name" || t == "role_name"; })) {
                nameAliases.insert(nameAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "permissions" || t == "perms"; })) {
                permAliases.insert(permAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "description" || t == "desc"; })) {
                descAliases.insert(descAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            }
        }
    }

    bool isName (const std::string& field) const { return isFieldMatch(field, nameAliases); }
    bool isPermissions (const std::string& field) const { return isFieldMatch(field, permAliases); }
    bool isDescription (const std::string& field) const { return isFieldMatch(field, descAliases); }
};

struct VaultRoleAliases {
    std::vector<std::string> nameAliases, permAliases, descAliases;

    explicit VaultRoleAliases(const std::shared_ptr<CLITestContext>& ctx) {
        const auto cmd = ctx->getCommand(EntityType::VAULT_ROLE, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for vault role update");
        for (const auto& opt : cmd->optional) {
            if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "name" || t == "role_name"; })) {
                nameAliases.insert(nameAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "permissions" || t == "perms"; })) {
                permAliases.insert(permAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "description" || t == "desc"; })) {
                descAliases.insert(descAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            }
        }
    }

    bool isName (const std::string& field) const { return isFieldMatch(field, nameAliases); }
    bool isPermissions (const std::string& field) const { return isFieldMatch(field, permAliases); }
    bool isDescription (const std::string& field) const { return isFieldMatch(field, descAliases); }
};

struct VaultAliases {
    std::vector<std::string> nameAliases, quotaAliases, ownerAliases, conflictPolicyAliases;

    explicit VaultAliases(const std::shared_ptr<CLITestContext>& ctx) {
        const auto cmd = ctx->getCommand(EntityType::VAULT, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for vault update");
        for (const auto& opt : cmd->optional) {
            if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "name" || t == "vault_name"; })) {
                nameAliases.insert(nameAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "description" || t == "desc"; })) {
                nameAliases.insert(nameAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "quota"; })) {
                quotaAliases.insert(quotaAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "owner" || t == "owner_id"; })) {
                ownerAliases.insert(ownerAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "conflict_policy" || t == "conflict"; })) {
                conflictPolicyAliases.insert(conflictPolicyAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            }
        }
    }

    bool isName (const std::string& field) const { return isFieldMatch(field, nameAliases); }
    bool isDescription (const std::string& field) const { return isFieldMatch(field, nameAliases); }
    bool isQuota (const std::string& field) const { return isFieldMatch(field, quotaAliases); }
    bool isOwner (const std::string& field) const { return isFieldMatch(field, ownerAliases); }
    bool isConflictPolicy (const std::string& field) const { return isFieldMatch(field, conflictPolicyAliases); }
};

struct S3VaultAliases : VaultAliases {
    std::vector<std::string> apiKeyAliases, syncStrategyAliases;

    explicit S3VaultAliases(const std::shared_ptr<CLITestContext>& ctx) : VaultAliases(ctx) {
        const auto cmd = ctx->getCommand(EntityType::VAULT, "update");
        if (!cmd) throw std::runtime_error("EntityFactory: command usage not found for vault update");
        for (const auto& opt : cmd->optional) {
            if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "api_key" || t == "api_key_id"; })) {
                apiKeyAliases.insert(apiKeyAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            } else if (std::ranges::any_of(opt.option_tokens, [](const std::string& t){ return t == "sync_strategy" || t == "sync"; })) {
                syncStrategyAliases.insert(syncStrategyAliases.end(), opt.option_tokens.begin(), opt.option_tokens.end());
            }
        }
    }

    bool isApiKey (const std::string& field) const { return isFieldMatch(field, apiKeyAliases); }
    bool isSyncStrategy (const std::string& field) const { return isFieldMatch(field, syncStrategyAliases); }
};

}

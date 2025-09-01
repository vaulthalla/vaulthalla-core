#include "protocols/shell/commands/vault.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"

#include "database/Queries/VaultQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "database/Queries/GroupQueries.hpp"

#include "types/Vault.hpp"
#include "types/Group.hpp"
#include "types/VaultRole.hpp"
#include "types/User.hpp"
#include "types/Role.hpp"

#include "services/LogRegistry.hpp"
#include "config/ConfigRegistry.hpp"

#include <optional>
#include <string>
#include <vector>
#include <memory>

using namespace vh::shell::commands;
using namespace vh::shell;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::database;
using namespace vh::config;
using namespace vh::services;
using namespace vh::crypto;
using namespace vh::util;
using namespace vh::logging;
using namespace vh::cloud;


static CommandResult handle_vault_role_assign(const CommandCall& call) {
    if (call.positionals.size() < 2) return invalid("vault assign: missing <vault_id> and <role_id>");
    if (call.positionals.size() > 2) return invalid("vault assign: too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto vaultIdOpt = parseInt(vaultArg);

    const auto roleArg = call.positionals.at(1);
    const auto roleIdOpt = parseInt(roleArg);

    std::shared_ptr<Vault> vault;

    if (vaultIdOpt) {
        if (*vaultIdOpt <= 0) return invalid("vault assign: vault ID must be a positive integer");
        vault = VaultQueries::getVault(*vaultIdOpt);
    } else {
        unsigned int ownerId;
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault assign: vault does not have a owner");
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault assign: --owner must be a positive integer");
            ownerId = *ownerIdOpt;
        } else if (ownerOpt) {
            if (ownerOpt->empty()) return invalid("vault assign: --owner requires a value");
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault assign: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        } else ownerId = call.user->id;
        vault = VaultQueries::getVault(vaultArg, ownerId);
    }

    if (!vault) return invalid("vault assign: vault with arg " + vaultArg + " not found");

    if (vault->owner_id != call.user->id) {
        if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id)) return invalid(
            "vault assign: you do not have permission to assign roles to this vault");

        if (!call.user->canManageRoles()) return invalid("vault assign: you do not have permission to manage roles");
    }

    std::shared_ptr<Role> role;

    if (roleIdOpt) {
        if (*roleIdOpt <= 0) return invalid("vault assign: role ID must be a positive integer");
        role = PermsQueries::getRole(*roleIdOpt);
    } else role = PermsQueries::getRoleByName(roleArg);

    if (!role) return invalid("vault assign: role with arg " + roleArg + " not found");

    std::string subjectType;
    unsigned int subjectId;

    if (const auto userOpt = optVal(call, "uid")) {
        subjectType = "user";
        if (const auto uidOpt = parseInt(*userOpt)) {
            if (*uidOpt <= 0) return invalid("vault assign: user ID must be a positive integer");
            subjectId = *uidOpt;
        } else {
            const auto user = UserQueries::getUserByName(*userOpt);
            if (!user) return invalid("vault assign: user not found: " + *userOpt);
            subjectId = user->id;
        }
    } else if (const auto groupOpt = optVal(call, "gid")) {
        subjectType = "group";
        if (const auto gidOpt = parseInt(*groupOpt)) {
            if (*gidOpt <= 0) return invalid("vault assign: group ID must be a positive integer");
            subjectId = *gidOpt;
        } else {
            const auto group = GroupQueries::getGroupByName(*groupOpt);
            if (!group) return invalid("vault assign: group not found: " + *groupOpt);
            subjectId = group->id;
        }
    } else return invalid("vault assign: must specify either --uid or --gid");

    const auto vr = std::make_shared<VaultRole>();
    vr->role_id = role->id;
    vr->vault_id = vault->id;
    vr->subject_type = subjectType;
    vr->subject_id = subjectId;

    PermsQueries::assignVaultRole(vr);

    return ok("Successfully assigned role '" + role->name + "' to vault '" + vault->name + "'");
}

static CommandResult handle_vault_role_remove(const CommandCall& call) {
    if (call.positionals.size() < 2) return invalid("vault remove: missing <vault_id> and <role_id>");
    if (call.positionals.size() > 2) return invalid("vault remove: too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto vaultIdOpt = parseInt(vaultArg);

    const auto roleArg = call.positionals.at(1);

    std::shared_ptr<Vault> vault;

    if (vaultIdOpt) {
        if (*vaultIdOpt <= 0) return invalid("vault remove: vault ID must be a positive integer");
        vault = VaultQueries::getVault(*vaultIdOpt);
    } else {
        unsigned int ownerId;
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault remove: vault does not have a owner");
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault remove: --owner must be a positive integer");
            ownerId = *ownerIdOpt;
        } else if (ownerOpt) {
            if (ownerOpt->empty()) return invalid("vault remove: --owner requires a value");
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault remove: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        } else ownerId = call.user->id;
        vault = VaultQueries::getVault(vaultArg, ownerId);
    }

    if (!vault) return invalid("vault remove: vault with arg " + vaultArg + " not found");

    if (vault->owner_id != call.user->id) {
        if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id)) return invalid(
            "vault remove: you do not have permission to remove roles from this vault");

        if (!call.user->canManageRoles()) return invalid("vault remove: you do not have permission to manage roles");
    }

    std::shared_ptr<Role> role;


    std::string subjectType;
    unsigned int subjectId;
}

static CommandResult handle_vault_role_list(const CommandCall& call) {
    if (call.positionals.size() > 1) return invalid("vault list: too many arguments");

    std::shared_ptr<Vault> vault;
    if (!call.positionals.empty()) {
        const auto vaultArg = call.positionals.at(0);
        const auto vaultIdOpt = parseInt(vaultArg);

        if (vaultIdOpt) {
            if (*vaultIdOpt <= 0) return invalid("vault list: vault ID must be a positive integer");
            vault = VaultQueries::getVault(*vaultIdOpt);
        } else {
            unsigned int ownerId;
            const auto ownerOpt = optVal(call, "owner");
            if (!ownerOpt) return invalid("vault list: vault does not have a owner");
            if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
                if (*ownerIdOpt <= 0) return invalid("vault list: --owner must be a positive integer");
                ownerId = *ownerIdOpt;
            } else if (ownerOpt) {
                if (ownerOpt->empty()) return invalid("vault list: --owner requires a value");
                const auto owner = UserQueries::getUserByName(*ownerOpt);
                if (!owner) return invalid("vault list: owner not found: " + *ownerOpt);
                ownerId = owner->id;
            } else ownerId = call.user->id;
            vault = VaultQueries::getVault(vaultArg, ownerId);
        }

        if (!vault) return invalid("vault list: vault with arg " + vaultArg + " not found");

        if (vault->owner_id != call.user->id) {
            if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id)) return invalid(
                "vault list: you do not have permission to view roles for this vault");
        }
    }

    std::vector<std::shared_ptr<VaultRole>> roles;
    if (vault) roles = PermsQueries::listVaultAssignedRoles(vault->id);
    else {
        if (!call.user->canManageRoles()) return invalid("vault list: you do not have permission to manage roles");
        roles = PermsQueries::listVaultAssignedRoles(0); // List all roles
    }

    nlohmann::json j;
    j["roles"] = roles;
    return ok(j.dump(4));
}

static CommandResult handle_vault_role_override(const CommandCall& call) {
    if (call.positionals.size() < 2) return invalid("vault override: missing <vault_id> and <role_id>");
    if (call.positionals.size() > 2) return invalid("vault override: too many arguments");

    const auto vaultArg = call.positionals.at(0);
    const auto vaultIdOpt = parseInt(vaultArg);

    const auto roleArg = call.positionals.at(1);

    std::shared_ptr<Vault> vault;

    if (vaultIdOpt) {
        if (*vaultIdOpt <= 0) return invalid("vault override: vault ID must be a positive integer");
        vault = VaultQueries::getVault(*vaultIdOpt);
    } else {
        unsigned int ownerId;
        const auto ownerOpt = optVal(call, "owner");
        if (!ownerOpt) return invalid("vault override: vault does not have a owner");
        if (const auto ownerIdOpt = parseInt(*ownerOpt)) {
            if (*ownerIdOpt <= 0) return invalid("vault override: --owner must be a positive integer");
            ownerId = *ownerIdOpt;
        } else if (ownerOpt) {
            if (ownerOpt->empty()) return invalid("vault override: --owner requires a value");
            const auto owner = UserQueries::getUserByName(*ownerOpt);
            if (!owner) return invalid("vault override: owner not found: " + *ownerOpt);
            ownerId = owner->id;
        } else ownerId = call.user->id;
        vault = VaultQueries::getVault(vaultArg, ownerId);
    }

    if (!vault) return invalid("vault override: vault with arg " + vaultArg + " not found");

    if (vault->owner_id != call.user->id) {
        if (!call.user->canManageVaults() && !call.user->canManageVaultAccess(vault->id)) return invalid(
            "vault override: you do not have permission to override roles for this vault");

        if (!call.user->canManageRoles()) return invalid("vault override: you do not have permission to manage roles");
    }

    std::shared_ptr<Role> role;

    if (const auto roleIdOpt = parseInt(roleArg)) role = PermsQueries::getRole(*roleIdOpt);
    else role = PermsQueries::getRoleByName(roleArg);
}


CommandResult commands::vault::handle_vault_role(const CommandCall& call) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;

    if (call.positionals.empty()) return usage(call.constructFullArgs());
    if (call.positionals.size() > 1) return invalid(call.constructFullArgs(), "vault role: too many arguments");

    const auto subcommand = call.positionals[0];
    auto subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());

    if (isCommandMatch({"vh", "vault", "role", "assign"}, subcommand)) return handle_vault_role_assign(subcall);
    if (isCommandMatch({"vh", "vault", "role", "remove"}, subcommand)) return handle_vault_role_remove(subcall);
    if (isCommandMatch({"vh", "vault", "role", "list"}, subcommand)) return handle_vault_role_list(subcall);
    if (isCommandMatch({"vh", "vault", "role", "override"}, subcommand)) return handle_vault_role_override(subcall);

    return invalid(call.constructFullArgs(), "Unknown vault role subcommand: '" + std::string(subcommand) + "'");
}

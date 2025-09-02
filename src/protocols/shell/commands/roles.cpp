#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "types/User.hpp"
#include "types/Role.hpp"
#include "types/VaultRole.hpp"
#include "types/UserRole.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "usage/include/UsageManager.hpp"

using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using namespace vh::services;

static uint16_t parseUserRolePermissions(const CommandCall& call, uint16_t permissions = 0) {
    if (hasFlag(call, "manage-encryption-keys") || hasFlag(call, "allow-manage-encryption-keys")) permissions |= (1 << 0);
    else if (hasFlag(call, "deny-manage-encryption-keys")) permissions &= ~(1 << 0);

    if (hasFlag(call, "manage-admins") || hasFlag(call, "allow-manage-admins")) permissions |= (1 << 1);
    else if (hasFlag(call, "deny-manage-admins")) permissions &= ~(1 << 1);

    if (hasFlag(call, "manage-users") || hasFlag(call, "allow-manage-users")) permissions |= (1 << 2);
    else if (hasFlag(call, "deny-manage-users")) permissions &= ~(1 << 2);

    if (hasFlag(call, "manage-groups") || hasFlag(call, "allow-manage-groups")) permissions |= (1 << 3);
    else if (hasFlag(call, "deny-manage-groups")) permissions &= ~(1 << 3);

    if (hasFlag(call, "manage-vaults") || hasFlag(call, "allow-manage-vaults")) permissions |= (1 << 4);
    else if (hasFlag(call, "deny-manage-vaults")) permissions &= ~(1 << 4);

    if (hasFlag(call, "manage-groups") || hasFlag(call, "allow-manage-groups")) permissions |= (1 << 5);
    else if (hasFlag(call, "deny-manage-groups")) permissions &= ~(1 << 5);

    if (hasFlag(call, "manage-roles") || hasFlag(call, "allow-manage-roles")) permissions |= (1 << 6);
    else if (hasFlag(call, "deny-manage-roles")) permissions &= ~(1 << 6);

    if (hasFlag(call, "manage-api-keys") || hasFlag(call, "allow-manage-api-keys")) permissions |= (1 << 7);
    else if (hasFlag(call, "deny-manage-api-keys")) permissions &= ~(1 << 7);

    if (hasFlag(call, "audit-log-access") || hasFlag(call, "allow-audit-log-access")) permissions |= (1 << 8);
    else if (hasFlag(call, "deny-audit-log-access")) permissions &= ~(1 << 8);

    if (hasFlag(call, "create-vaults") || hasFlag(call, "allow-create-vaults")) permissions |= (1 << 9);
    else if (hasFlag(call, "deny-create-vaults")) permissions &= ~(1 << 9);

    return permissions;
}

static uint16_t parseVaultRolePermissions(const CommandCall& call, uint16_t permissions = 0) {
    if (hasFlag(call, "manage-vault") || hasFlag(call, "allow-manage-vault")) permissions |= (1 << 0);
    else if (hasFlag(call, "deny-manage-vault")) permissions &= ~(1 << 0);

    if (hasFlag(call, "manage-access") || hasFlag(call, "allow-manage-access")) permissions |= (1 << 1);
    else if (hasFlag(call, "deny-manage-access")) permissions &= ~(1 << 1);

    if (hasFlag(call, "manage-tags") || hasFlag(call, "allow-manage-tags")) permissions |= (1 << 2);
    else if (hasFlag(call, "deny-manage-tags")) permissions &= ~(1 << 2);

    if (hasFlag(call, "manage-metadata") || hasFlag(call, "allow-manage-metadata")) permissions |= (1 << 3);
    else if (hasFlag(call, "deny-manage-metadata")) permissions &= ~(1 << 3);

    if (hasFlag(call, "manage-versions") || hasFlag(call, "allow-manage-versions")) permissions |= (1 << 4);
    else if (hasFlag(call, "deny-manage-versions")) permissions &= ~(1 << 4);

    if (hasFlag(call, "manage-file-locks") || hasFlag(call, "allow-manage-file-locks")) permissions |= (1 << 5);
    else if (hasFlag(call, "deny-manage-file-locks")) permissions &= ~(1 << 5);

    if (hasFlag(call, "share") || hasFlag(call, "allow-share")) permissions |= (1 << 6);
    else if (hasFlag(call, "deny-share")) permissions &= ~(1 << 6);

    if (hasFlag(call, "sync") || hasFlag(call, "allow-sync")) permissions |= (1 << 7);
    else if (hasFlag(call, "deny-sync")) permissions &= ~(1 << 7);

    if (hasFlag(call, "create") || hasFlag(call, "allow-create")) permissions |= (1 << 8);
    else if (hasFlag(call, "deny-create")) permissions &= ~(1 << 8);

    if (hasFlag(call, "download") || hasFlag(call, "allow-download")) permissions |= (1 << 9);
    else if (hasFlag(call, "deny-download")) permissions &= ~(1 << 9);

    if (hasFlag(call, "delete") || hasFlag(call, "allow-delete")) permissions |= (1 << 10);
    else if (hasFlag(call, "deny-delete")) permissions &= ~(1 << 10);

    if (hasFlag(call, "rename") || hasFlag(call, "allow-rename")) permissions |= (1 << 11);
    else if (hasFlag(call, "deny-rename")) permissions &= ~(1 << 11);

    if (hasFlag(call, "move") || hasFlag(call, "allow-move")) permissions |= (1 << 12);
    else if (hasFlag(call, "deny-move")) permissions &= ~(1 << 12);

    if (hasFlag(call, "list") || hasFlag(call, "allow-list")) permissions |= (1 << 13);
    else if (hasFlag(call, "deny-list")) permissions &= ~(1 << 13);

    return permissions;
}

static CommandResult handleRolesList(const CommandCall& call) {
    int limit = 100; // Default limit
    if (auto lim = optVal(call, "limit")) {
        if (lim->empty()) return invalid("roles list: --limit requires a value");
        auto parsed = parseInt(*lim);
        if (!parsed || *parsed <= 0) return invalid("roles list: --limit must be a positive integer");
        limit = *parsed;
    }

    const auto roles = PermsQueries::listRoles();
    return ok(to_string(roles));
}

static CommandResult handleRoleInfo(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("roles info: missing <id> or <name>");
    if (call.positionals.size() > 1) return invalid("roles info: too many arguments");

    const auto& arg = call.positionals[0];
    const auto roleIdOpt = parseInt(arg);
    std::shared_ptr<Role> role;

    if (roleIdOpt) {
        role = PermsQueries::getRole(*roleIdOpt);
        if (!role) return invalid("roles info: role with ID " + std::to_string(*roleIdOpt) + " not found");
    } else {
        role = PermsQueries::getRoleByName(std::string(arg));
        if (!role) return invalid("roles info: role with name '" + std::string(arg) + "' not found");
    }

    return ok(to_string(role));
}

static std::shared_ptr<Role> resolveFromRoleSameType(const std::string& from,
                                                     std::string_view expected_type) {
    std::shared_ptr<Role> r;
    if (const auto idOpt = parseInt(from)) r = PermsQueries::getRole(*idOpt);
    else r = PermsQueries::getRoleByName(from);

    if (!r) throw std::runtime_error("role with name or id '" + from + "' not found");
    if (r->type != expected_type)
        throw std::runtime_error("mismatched --from type: expected '" +
                                 std::string(expected_type) + "', got '" + r->type + "'");
    return r;
}

static std::optional<int> optIntFlag(const CommandCall& call, std::string key) {
    if (auto v = optVal(call, key)) {
        if (auto i = parseInt(*v)) return *i;
        throw std::runtime_error("invalid integer for --" + std::string(key) + ": " + std::string(*v));
    }
    return std::nullopt;
}

static CommandResult handleRoleCreateUser(const CommandCall& call, std::string_view name) {
    try {
        uint16_t base = 0;
        if (const auto fromOpt = optVal(call, "from")) {
            const auto from = resolveFromRoleSameType(*fromOpt, "user");
            base = from->permissions;
        }
        const uint16_t perms = parseUserRolePermissions(call, base);

        const auto role = std::make_shared<UserRole>();
        role->name = std::string(name);
        role->type = "user";
        role->permissions = perms;

        PermsQueries::addRole(role);
        return ok("Role created successfully: " + to_string(role));
    } catch (const std::exception& e) {
        return invalid(std::string("roles create user: ") + e.what());
    }
}

static CommandResult handleRoleCreateVault(const CommandCall& call, const std::string_view name) {
    try {
        uint16_t base = 0;
        if (const auto fromOpt = optVal(call, "from")) {
            const auto from = resolveFromRoleSameType(*fromOpt, "vault");
            base = from->permissions;
        }
        const uint16_t perms = parseVaultRolePermissions(call, base);

        const auto role = std::make_shared<VaultRole>();
        role->name = std::string(name);
        role->type = "vault";
        role->permissions = perms;

        if (hasFlag(call, "uid")) {
            if (hasFlag(call, "gid")) return invalid("roles create vault: cannot specify both --uid and --gid");
            role->subject_type = "user";
            const auto uidOpt = optIntFlag(call, "uid");
            if (!uidOpt) return invalid("roles create vault: missing required --uid <uid>");
            role->subject_id = *uidOpt;
        } else if (hasFlag(call, "gid")) {
            if (hasFlag(call, "uid")) return invalid("roles create vault: cannot specify both --uid and --gid");
            role->subject_type = "group";
            const auto gidOpt = optIntFlag(call, "gid");
            if (!gidOpt) return invalid("roles create vault: missing required --gid <gid>");
            role->subject_id = *gidOpt;
        } else return invalid("roles create vault: must specify either --uid or --gid");

        if (const auto vid = optIntFlag(call, "vault-id")) role->vault_id = *vid;
        else return invalid("roles create vault: missing required --vault-id <vid>");

        PermsQueries::addRole(role);
        return ok("Role created successfully: " + to_string(role));
    } catch (const std::exception& e) {
        return invalid(std::string("roles create vault: ") + e.what());
    }
}

static CommandResult handleRoleCreate(const CommandCall& call) {
    if (call.positionals.size() < 2)
        return invalid("roles create: missing <vault | user> <name>");

    const auto type = std::string(call.positionals[0]);
    const auto name = std::string(call.positionals[1]);

    if (type == "user")  return handleRoleCreateUser(call, name);
    if (type == "vault") return handleRoleCreateVault(call, name);

    return invalid("roles create: first argument must be 'vault' or 'user'");
}

static CommandResult handleRoleUpdate(const CommandCall& call) {
    if (!call.user->canManageRoles()) return invalid("roles update: can't manage");

    if (call.positionals.empty()) return invalid("roles update: missing <id> or <name>");
    if (call.positionals.size() > 1) return invalid("roles update: too many arguments");

    const auto roleId = call.positionals[0];
    auto roleIdOpt = parseInt(roleId);
    if (!roleIdOpt) return invalid("roles update: invalid role ID '" + roleId + "'");

    const auto role = PermsQueries::getRole(*roleIdOpt);
    if (!role) return invalid("roles update: role with ID " + std::to_string(*roleIdOpt) + " not found");

    if (role->type == "user" && !call.user->canManageUsers())
        return invalid("roles update: you do not have permission to update user roles");

    if (role->type == "vault" && !call.user->canManageVaults())
        return invalid("roles update: you do not have permission to update vault roles");

    role->permissions = role->type == "user"
        ? parseUserRolePermissions(call, role->permissions)
        : parseVaultRolePermissions(call, role->permissions);

    if (role->type == "vault") {
        const auto vaultRole = std::static_pointer_cast<VaultRole>(role);
        if (hasFlag(call, "uid")) {
            if (hasFlag(call, "gid")) return invalid("roles update: cannot specify both --uid and --gid");
            vaultRole->subject_type = "user";
            const auto uidOpt = optIntFlag(call, "uid");
            if (!uidOpt) return invalid("roles update: missing required --uid <uid>");
            vaultRole->subject_id = *uidOpt;
        } else if (hasFlag(call, "gid")) {
            if (hasFlag(call, "uid")) return invalid("roles update: cannot specify both --uid and --gid");
            vaultRole->subject_type = "group";
            const auto gidOpt = optIntFlag(call, "gid");
            if (!gidOpt) return invalid("roles update: missing required --gid <gid>");
            vaultRole->subject_id = *gidOpt;
        }

        if (const auto vid = optIntFlag(call, "vault-id")) vaultRole->vault_id = *vid;
    }

    PermsQueries::updateRole(role);

    return ok("Role updated successfully:\n" + to_string(role));
}

static CommandResult handleRoleDelete(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("roles delete: missing <id> or <name>");
    if (call.positionals.size() > 1) return invalid("roles delete: too many arguments");

    const auto arg = call.positionals[0];
    const auto roleIdOpt = parseInt(arg);
    std::shared_ptr<Role> role;

    if (roleIdOpt) {
        role = PermsQueries::getRole(*roleIdOpt);
        if (!role) return invalid("roles delete: role with ID " + std::to_string(*roleIdOpt) + " not found");
    } else {
        role = PermsQueries::getRoleByName(std::string(arg));
        if (!role) return invalid("roles delete: role with name '" + std::string(arg) + "' not found");
    }

    try {
        PermsQueries::deleteRole(role->id);
        return ok("Role deleted successfully: " + to_string(role));
    } catch (const std::exception& e) {
        return invalid("roles delete: " + std::string(e.what()));
    }
}

static CommandResult handle_role(const CommandCall& call) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;
    if (call.positionals.empty()) return usage(call.constructFullArgs());
    const std::string_view sub = call.positionals[0];
    CommandCall subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());

    if (sub == "create") return handleRoleCreate(subcall);
    if (sub == "delete") return handleRoleDelete(subcall);
    if (sub == "info") return handleRoleInfo(subcall);
    if (sub == "update") return handleRoleUpdate(subcall);

    return invalid(call.constructFullArgs(), "Unknown roles subcommand: '" + std::string(sub) + "'");
}

void commands::registerRoleCommands(const std::shared_ptr<Router>& r) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;
    r->registerCommand(usageManager->resolve("role"), handle_role);
}

#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/Router.hpp"
#include "util/shellArgsHelpers.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "protocols/shell/commands/vault.hpp"
#include "types/User.hpp"
#include "types/Role.hpp"
#include "types/VaultRole.hpp"
#include "types/UserRole.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "usage/include/UsageManager.hpp"

using namespace vh::shell::commands;
using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using namespace vh::services;

static void parseFlag(const CommandCall& call, const std::string& flag, uint16_t& permissions, const unsigned int bitPosition) {
    if (hasFlag(call, flag) || hasFlag(call, "allow-" + flag)) permissions |= (1 << bitPosition);
    else if (hasFlag(call, "deny-" + flag)) permissions &= ~(1 << bitPosition);
}

static uint16_t parseUserRolePermissions(const CommandCall& call, uint16_t permissions = 0) {
    parseFlag(call, "manage-encryption-keys", permissions, 0);
    parseFlag(call, "manage-admins", permissions, 1);
    parseFlag(call, "manage-users", permissions, 2);
    parseFlag(call, "manage-groups", permissions, 3);
    parseFlag(call, "manage-roles", permissions, 4);
    parseFlag(call, "manage-settings", permissions, 5);
    parseFlag(call, "manage-vaults", permissions, 6);
    parseFlag(call, "manage-api-keys", permissions, 7);
    parseFlag(call, "audit-log-access", permissions, 8);
    parseFlag(call, "create-vaults", permissions, 9);

    return permissions;
}

static uint16_t parseVaultRolePermissions(const CommandCall& call, uint16_t permissions = 0) {
    parseFlag(call, "manage-vault", permissions, 0);
    parseFlag(call, "manage-access", permissions, 1);
    parseFlag(call, "manage-tags", permissions, 2);
    parseFlag(call, "manage-metadata", permissions, 3);
    parseFlag(call, "manage-versions", permissions, 4);
    parseFlag(call, "manage-file-locks", permissions, 5);
    parseFlag(call, "share", permissions, 6);
    parseFlag(call, "sync", permissions, 7);
    parseFlag(call, "create", permissions, 8);
    parseFlag(call, "download", permissions, 9);
    parseFlag(call, "delete", permissions, 10);
    parseFlag(call, "rename", permissions, 11);
    parseFlag(call, "move", permissions, 12);
    parseFlag(call, "list", permissions, 13);

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
                                                     const std::string_view expected_type) {
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
    constexpr const auto* ERR = "roles update";

    if (!call.user->canManageRoles()) return invalid("roles update: can't manage");

    if (call.positionals.empty()) return invalid("roles update: missing <id> or <name>");
    if (call.positionals.size() > 1) return invalid("roles update: too many arguments");

    const auto roleArg = call.positionals[0];

    const auto rLkp = resolveRole(roleArg, ERR);
    if (!rLkp || !rLkp.ptr) return invalid(rLkp.error);
    const auto role = rLkp.ptr;

    if (role->type == "user" && !call.user->canManageUsers())
        return invalid("roles update: you do not have permission to update user roles");

    if (role->type == "vault" && !call.user->canManageVaults())
        return invalid("roles update: you do not have permission to update vault roles");

    role->permissions = role->type == "user"
        ? parseUserRolePermissions(call, role->permissions)
        : parseVaultRolePermissions(call, role->permissions);

    if (role->type == "vault") {
        const auto vaultRole = std::static_pointer_cast<VaultRole>(role);

        const auto sLkp = parseSubject(call, ERR);
        if (!sLkp) return invalid(sLkp.error);
        const auto [subjectType, subjectId] = *sLkp.ptr;

        vaultRole->subject_type = subjectType;
        vaultRole->subject_id = subjectId;

        if (const auto vid = optIntFlag(call, "vault-id")) vaultRole->vault_id = *vid;
    }

    PermsQueries::updateRole(role);

    return ok("Role updated successfully:\n" + to_string(role));
}

static CommandResult handleRoleDelete(const CommandCall& call) {
    constexpr const auto* ERR = "roles delete";

    if (call.positionals.empty()) return invalid("roles delete: missing <id> or <name>");
    if (call.positionals.size() > 1) return invalid("roles delete: too many arguments");

    const auto roleArg = call.positionals[0];

    const auto rLkp = resolveRole(roleArg, ERR);
    if (!rLkp || !rLkp.ptr) return invalid(rLkp.error);
    const auto role = rLkp.ptr;

    try {
        PermsQueries::deleteRole(role->id);
        return ok("Role deleted successfully: " + to_string(role));
    } catch (const std::exception& e) {
        return invalid("roles delete: " + std::string(e.what()));
    }
}

static bool isRoleMatch(const std::string& cmd, const std::string_view input) {
    return isCommandMatch({"role", cmd}, input);
}

static CommandResult handle_role(const CommandCall& call) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;
    if (call.positionals.empty()) return usage(call.constructFullArgs());

    const auto [sub, subcall] = descend(call);

    if (isRoleMatch("create", sub)) return handleRoleCreate(subcall);
    if (isRoleMatch("update", sub)) return handleRoleUpdate(subcall);
    if (isRoleMatch("delete", sub)) return handleRoleDelete(subcall);
    if (isRoleMatch("info", sub)) return handleRoleInfo(subcall);
    if (isRoleMatch("list", sub)) return handleRolesList(subcall);

    return invalid(call.constructFullArgs(), "Unknown roles subcommand: '" + std::string(sub) + "'");
}

void commands::registerRoleCommands(const std::shared_ptr<Router>& r) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;
    r->registerCommand(usageManager->resolve("role"), handle_role);
}

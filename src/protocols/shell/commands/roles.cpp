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
#include "CommandUsage.hpp"

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
    if (!call.user->canManageRoles()) return invalid("roles list: can't manage");

    auto params = parseListQuery(call);
    std::vector<std::shared_ptr<Role>> roles;

    if (hasFlag(call, "user")) roles = PermsQueries::listUserRoles(std::move(params));
    else if (hasFlag(call, "vault")) roles = PermsQueries::listVaultRoles(std::move(params));
    else roles = PermsQueries::listRoles(std::move(params));

    return ok(to_string(roles));
}

static CommandResult handleRoleInfo(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("roles info: missing <id> or <name>");
    if (call.positionals.size() > 1) return invalid("roles info: too many arguments");

    const auto& arg = call.positionals[0];
    std::shared_ptr<Role> role;

    if (const auto roleIdOpt = parseUInt(arg)) {
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
    if (const auto idOpt = parseUInt(from)) r = PermsQueries::getRole(*idOpt);
    else r = PermsQueries::getRoleByName(from);

    if (!r) throw std::runtime_error("role with name or id '" + from + "' not found");
    if (r->type != expected_type)
        throw std::runtime_error("mismatched --from type: expected '" +
                                 std::string(expected_type) + "', got '" + r->type + "'");
    return r;
}

static CommandResult handleRoleCreate(const CommandCall& call) {
    const std::vector<std::string> callPath{"role", "create"};
    const auto usage = ServiceDepsRegistry::instance().shellUsageManager->resolve(callPath);
    if (!usage) exit(33); // should never happen

    if (call.positionals.size() != usage->positionals.size())
        return invalid("roles create: incorrect number of arguments");

    const auto type = call.positionals[0];
    const auto name = call.positionals[1];

    if (type != "user" && type != "vault")
        return invalid("roles create: type must be either 'user' or 'vault'");

    uint16_t base = 0;
    const auto fromAliases = usage->resolveOptional("from")->option_tokens;
    if (const auto fromOpt = optVal(call, fromAliases)) {
        const auto from = resolveFromRoleSameType(*fromOpt, type);
        base = from->permissions;
    }

    const auto role = std::make_shared<Role>();
    role->name = name,
    role->type = type;
    role->permissions = type == "user"
        ? parseUserRolePermissions(call, base)
        : parseVaultRolePermissions(call, base);

    const auto descAliases = usage->resolveOptional("description")->option_tokens;
    if (const auto descOpt = optVal(call, descAliases))
        role->description = *descOpt;

    role->id = PermsQueries::addRole(role);

    return ok("Role created successfully: " + to_string(role));
}

static CommandResult handleRoleUpdate(const CommandCall& call) {
    constexpr const auto* ERR = "roles update";

    const std::vector<std::string> callPath{"role", "update"};
    const auto usage = ServiceDepsRegistry::instance().shellUsageManager->resolve(callPath);
    if (!usage) throw std::runtime_error("No usage found for 'role update'");

    if (call.positionals.size() != usage->positionals.size())
        return invalid("roles update: incorrect number of arguments");

    if (!call.user->canManageRoles()) return invalid("roles update: can't manage");

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

    const auto nameAliases = usage->resolveOptional("name")->option_tokens;
    if (const auto newNameOpt = optVal(call, nameAliases)) {
        if (newNameOpt->empty()) return invalid("roles update: --name cannot be empty");
        role->name = *newNameOpt;
    }

    const auto descAliases = usage->resolveOptional("description")->option_tokens;
    if (const auto descriptionOpt = optVal(call, descAliases)) role->description = *descriptionOpt;

    role->permissions = role->type == "user"
        ? parseUserRolePermissions(call, role->permissions)
        : parseVaultRolePermissions(call, role->permissions);

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

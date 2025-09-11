#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/Router.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "util/shellArgsHelpers.hpp"
#include "auth/AuthManager.hpp"
#include "crypto/PasswordHash.hpp"
#include "logging/LogRegistry.hpp"
#include "types/UserRole.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "usage/include/UsageManager.hpp"

using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using namespace vh::auth;
using namespace vh::services;
using namespace vh::logging;
using namespace vh::util;
using namespace vh::crypto;

static CommandResult createUser(const CommandCall& subcall) {
    constexpr const auto* ERR = "user create";

    if (subcall.positionals.empty()) return invalid("Usage: user create <name> [--email email] [--linux-uid uid] [--role role]");

    if (!subcall.user->canManageUsers()) return invalid("You do not have permission to create users.");

    const auto name = subcall.positionals[0];

    const auto emailOpt = optVal(subcall, "email");
    const auto linuxUidOpt = optVal(subcall, "linux-uid");
    auto roleOpt = optVal(subcall, "role");

    std::vector<std::string> errors;
    if (!roleOpt) roleOpt = optVal(subcall, "r");
    if (!roleOpt) errors.emplace_back("Missing required option: --role");
    if (roleOpt && roleOpt->empty()) errors.emplace_back("Option --role cannot be empty");

    if (!errors.empty()) {
        std::string errorMsg = "User creation failed:\n";
        for (const auto& err : errors) errorMsg += "  - " + err + "\n";
        return invalid(errorMsg);
    }

    const auto user = std::make_shared<User>();
    user->name = name;
    user->email = emailOpt;
    if (linuxUidOpt) user->linux_uid = parseInt(*linuxUidOpt);
    user->role = std::make_shared<UserRole>();
    user->last_modified_by = subcall.user->id;

    if (!AuthManager::isValidName(user->name)) return invalid("Invalid user name: " + user->name);

    const auto rLkp = resolveRole(*roleOpt, ERR);
    if (!rLkp || !rLkp.ptr) return invalid(rLkp.error);
    const auto role = rLkp.ptr;

    user->role->id = role->id;
    user->role->name = role->name;
    user->role->permissions = role->permissions;

    constexpr unsigned short maxRetries = 1024;
    std::string password;
    for (unsigned short i = 1; i < maxRetries; ++i) {
        password = generate_secure_password(84);
        if (AuthManager::isValidPassword(password)) {
            try {
                AuthManager::isValidRegistration(user, password);
                break; // Valid password found
            } catch (const std::exception& e) {
                LogRegistry::auth()->warn("[AuthManager] Password validation failed: {}", e.what());
            }
        }
        if (i == maxRetries)
            return invalid("Failed to generate a valid password after " + std::to_string(maxRetries) + " attempts.");
    }

    try {
        user->setPasswordHash(hashPassword(password));
        user->id = UserQueries::createUser(user);

        std::string out = "User created successfully: " + to_string(user) + "\n";
        out += "Password: " + password + "\n";
        return ok(out);

    } catch (const std::exception& e) {
        return invalid("User creation failed: " + std::string(e.what()));
    }
}

static CommandResult deleteUser(const CommandCall& subcall) {
    try {
        auto name = subcall.positionals.empty() ? "" : subcall.positionals[0];
        if (name.empty()) {
            const auto nameOpt = optVal(subcall, "name");
            if (!nameOpt || nameOpt->empty()) return invalid("Usage: user delete <name>");
            name = *nameOpt;
        }

        if (name.empty()) return invalid("Usage: user delete <name>");

        const auto user = UserQueries::getUserByName(std::string(name));
        if (!user) return invalid("User not found: " + std::string(name));

        if (user->isSuperAdmin()) {
            LogRegistry::audit()->warn("[UserCommands] Attempt to delete super_admin user: {}, by user: {}",
                user->name, subcall.user->name);
            LogRegistry::shell()->warn("[UserCommands] Attempt to delete super_admin user: {}, by user: {}",
                user->name, subcall.user->name);
            LogRegistry::vaulthalla()->warn("[UserCommands] Attempt to delete super_admin user: {}, by user: {}",
                user->name, subcall.user->name);
            return invalid("Cannot delete super admin user: " + user->name);
        }

        if (subcall.user->id != user->id) {
            if (!subcall.user->canManageUsers())
                return invalid("You do not have permission to delete users.");
            if (user->isAdmin() && !subcall.user->canManageAdmins())
                return invalid("You do not have permission to delete admin users.");
        }

        UserQueries::deleteUser(user->id);
        return ok("User deleted successfully: " + user->name);
    } catch (const std::exception& e) {
        return invalid("Failed to delete user: " + std::string(e.what()));
    }
}

static CommandResult handleUserInfo(const CommandCall& subcall) {
    constexpr const auto* ERR = "user info";

    if (subcall.positionals.empty()) return invalid("Usage: user info <name>");

    const auto userArg = subcall.positionals[0];

    const auto uLkp = resolveUser(userArg, ERR);
    if (!uLkp || !uLkp.ptr) return invalid(uLkp.error);
    const auto user = uLkp.ptr;

    if (subcall.user->id != user->id) {
        if (user->isSuperAdmin())
            return invalid("Cannot view super admin user: " + user->name);
        if (!subcall.user->canManageUsers())
            return invalid("You do not have permission to view other users.");
        if (user->isAdmin() && !subcall.user->canManageAdmins())
            return invalid("You do not have permission to view admin users.");
    }

    return ok(to_string(user));
}

static CommandResult handleUpdateUser(const CommandCall& subcall) {
    constexpr const auto* ERR = "user update";

    if (subcall.positionals.empty()) return invalid("Usage: user update <name> [--name <new_name>] [--email <email>] [--role <role>] [--linux-uid <uid>]");

    const auto name = subcall.positionals[0];
    const auto user = UserQueries::getUserByName(name);
    if (!user) return invalid("User not found: " + name);

    if (subcall.user->id != user->id) {
        if (user->isSuperAdmin())
            return invalid("Cannot update super admin user: " + user->name);

        if (!subcall.user->canManageUsers())
            return invalid("You do not have permission to update other users.");

        if (user->isAdmin() && !subcall.user->canManageAdmins())
            return invalid("You do not have permission to update admin users.");
    }

    if (const auto newNameOpt = optVal(subcall, "name")) {
        if (user->isSuperAdmin()) return invalid("Cannot change name of super_admin user: " + user->name);
        if (!AuthManager::isValidName(*newNameOpt)) return invalid("Invalid new user name: " + *newNameOpt);
        user->name = *newNameOpt;
    }

    if (const auto newEmailOpt = optVal(subcall, "email")) {
        if (!AuthManager::isValidEmail(*newEmailOpt)) return invalid("Invalid email address: " + *newEmailOpt);
        user->email = *newEmailOpt;
    }

    if (const auto newRoleOpt = optVal(subcall, "role")) {
        if (user->isSuperAdmin()) return invalid("Cannot change role of super_admin user: " + user->name);
        if (*newRoleOpt == "super_admin") return invalid("Cannot change role to super_admin.");

        const auto rLkp = resolveRole(*newRoleOpt, ERR);
        if (!rLkp || !rLkp.ptr) return invalid(rLkp.error);
        const auto role = rLkp.ptr;
        user->role->id = role->id;
    }

    if (const auto newLinuxUidOpt = optVal(subcall, "linux-uid")) {
        const auto linuxUid = parseInt(*newLinuxUidOpt);
        if (!linuxUid) return invalid("Invalid Linux UID: " + *newLinuxUidOpt);
        user->linux_uid = linuxUid;
    }

    user->last_modified_by = subcall.user->id;

    UserQueries::updateUser(user);

    return ok("User updated successfully: " + user->name + "\n" + to_string(user));
}

static CommandResult handle_list_users(const CommandCall& call) {
    if (!call.user->canManageUsers()) return invalid("You do not have permission to list users.");
    return ok(to_string(UserQueries::listUsers(parseListQuery(call))));
}

static bool isUserMatch(const std::string& cmd, const std::string_view input) {
    return isCommandMatch({"user", cmd}, input);
}

static CommandResult handle_user(const CommandCall& call) {
    if (call.positionals.empty() || hasFlag(call, "h") || hasFlag(call, "help"))
        return usage(call.constructFullArgs());

    const auto [sub, subcall] = descend(call);

    if (isUserMatch("create", sub)) return createUser(subcall);
    if (isUserMatch("delete", sub)) return deleteUser(subcall);
    if (isUserMatch("info", sub)) return handleUserInfo(subcall);
    if (isUserMatch("update", sub)) return handleUpdateUser(subcall);
    if (isUserMatch("list", sub) || isUserMatch("ls", sub)) return handle_list_users(subcall);

    return invalid(call.constructFullArgs(), "Unknown user subcommand: '" + std::string(sub) + "'");
}

void commands::registerUserCommands(const std::shared_ptr<Router>& r) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;
    r->registerCommand(usageManager->resolve("user"), handle_user);
}

#include "protocols/shell/commands/user.hpp"
#include "protocols/shell/Router.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "auth/AuthManager.hpp"
#include "storage/StorageEngine.hpp"
#include "crypto/PasswordHash.hpp"
#include "logging/LogRegistry.hpp"
#include "types/UserRole.hpp"

using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using namespace vh::auth;
using namespace vh::services;
using namespace vh::logging;

static CommandResult usage_user_root() {
    return {
        0,
        "Usage:\n"
        "  user [create | new] --name <name> --role <role> [--email <email>] [--linux-uid <uid>]\n"
        "  user [delete | rm] <name>\n"
        "  user [info | get] <name> \n"
        "  user [update | set] <name> [--name <name>] [--email <email>] [--role <role>] [--linux-uid <uid>]\n",
        ""
    };
}

static CommandResult createUser(const CommandCall& subcall) {
    if (!subcall.user->canManageUsers()) return invalid("You do not have permission to create users.");

    const auto nameOpt = optVal(subcall, "name");
    const auto emailOpt = optVal(subcall, "email");
    const auto linuxUidOpt = optVal(subcall, "linux-uid");
    const auto roleOpt = optVal(subcall, "role");

    std::vector<std::string> errors;
    if (!nameOpt) errors.emplace_back("Missing required option: --name");
    if (nameOpt && nameOpt->empty()) errors.emplace_back("Option --name cannot be empty");
    if (!roleOpt) errors.emplace_back("Missing required option: --role");
    if (roleOpt && roleOpt->empty()) errors.emplace_back("Option --role cannot be empty");

    if (!errors.empty()) {
        std::string errorMsg = "User creation failed:\n";
        for (const auto& err : errors) errorMsg += "  - " + err + "\n";
        return invalid(errorMsg);
    }

    const auto user = std::make_shared<User>();
    user->name = *nameOpt;
    user->email = emailOpt;
    if (linuxUidOpt) user->linux_uid = parseInt(*linuxUidOpt);
    user->role = std::make_shared<UserRole>();
    user->last_modified_by = subcall.user->id;

    if (!AuthManager::isValidName(user->name)) return invalid("Invalid user name: " + user->name);

    if (const auto rInt = parseInt(*roleOpt)) {
        if (*rInt == PermsQueries::getRoleByName("super_admin")->id)
            return invalid("Cannot create user with super admin role.");

        if (*rInt == PermsQueries::getRoleByName("admin")->id && !subcall.user->canManageAdmins())
            return invalid("You do not have permission to create admin users.");

        const auto role = PermsQueries::getRole(*rInt);
        if (!role) return invalid("Invalid role ID: " + std::to_string(*rInt));
        user->role->id = role->id;
        user->role->name = role->name;
        user->role->permissions = role->permissions;
    } else {
        if (*roleOpt == "super_admin")
            return invalid("Cannot create user with super admin role.");

        if (*roleOpt == "admin" && !subcall.user->canManageAdmins())
            return invalid("You do not have permission to create admin users.");

        const auto role = PermsQueries::getRoleByName(std::string(*roleOpt));
        if (!role) return invalid("Invalid role name: " + std::string(*roleOpt));
        user->role->id = role->id;
        user->role->name = role->name;
        user->role->permissions = role->permissions;
    }

    constexpr unsigned short maxRetries = 1024;
    std::string password;
    for (unsigned short i = 1; i < maxRetries; ++i) {
        password = vh::crypto::generate_secure_password(84);
        if (AuthManager::isValidPassword(password)) {
            try {
                AuthManager::isValidRegistration(user, password);
                break; // Valid password found
            } catch (const std::exception& e) {
                LogRegistry::auth()->warn("[AuthManager] Password validation failed: {}", e.what());
            }
        }
        if (i == maxRetries)
            return invalid(
                "Failed to generate a valid password after " + std::to_string(maxRetries) +
                " attempts.");
    }

    try {
        user->setPasswordHash(vh::crypto::hashPassword(password));
        user->id = UserQueries::createUser(user);

        std::string out = "User created successfully: " + user->name + "\n";
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
    auto name = subcall.positionals.empty() ? "" : subcall.positionals[0];
    const auto nameOpt = optVal(subcall, "name");
    const auto idOpt = optVal(subcall, "id");

    std::shared_ptr<User> user;

    if (!name.empty()) {
        user = UserQueries::getUserByName(name);
        if (!user) return invalid("User not found: " + name);
    } else if (nameOpt) {
        name = *nameOpt;
        user = UserQueries::getUserByName(name);
        if (!user) return invalid("User not found: " + name);
    } else if (idOpt) {
        const auto id = parseInt(*idOpt);
        if (!id) return invalid("Invalid user ID: " + *idOpt);
        user = UserQueries::getUserById(*id);
        if (!user) return invalid("User not found with ID: " + std::to_string(*id));
    } else return invalid("Usage: user info <name> | --id <user_id>");

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

    const auto newNameOpt = optVal(subcall, "name");
    const auto newEmailOpt = optVal(subcall, "email");
    const auto newRoleOpt = optVal(subcall, "role");
    const auto newLinuxUidOpt = optVal(subcall, "linux-uid");

    if (newNameOpt) {
        if (user->isSuperAdmin()) return invalid("Cannot change name of super_admin user: " + user->name);
        if (!AuthManager::isValidName(*newNameOpt)) return invalid("Invalid new user name: " + *newNameOpt);
        user->name = *newNameOpt;
    }

    if (newEmailOpt) {
        if (!AuthManager::isValidEmail(*newEmailOpt)) return invalid("Invalid email address: " + *newEmailOpt);
        user->email = *newEmailOpt;
    }

    if (newRoleOpt) {
        if (user->isSuperAdmin())
            return invalid("Cannot change role of super_admin user: " + user->name);

        if (const auto rInt = parseInt(*newRoleOpt)) {
            if (*rInt == PermsQueries::getRoleByName("super_admin")->id)
                return invalid("Cannot change role to super_admin.");

            const auto role = PermsQueries::getRole(*rInt);
            if (!role) return invalid("Invalid role ID: " + std::to_string(*rInt));
            user->role->id = role->id;
        } else {
            if (*newRoleOpt == "super_admin") return invalid("Cannot change role to super_admin.");
            const auto role = PermsQueries::getRoleByName(std::string(*newRoleOpt));
            if (!role) return invalid("Invalid role name: " + std::string(*newRoleOpt));
            user->role->id = role->id;
        }
    }

    if (newLinuxUidOpt) {
        const auto linuxUid = parseInt(*newLinuxUidOpt);
        if (!linuxUid) return invalid("Invalid Linux UID: " + *newLinuxUidOpt);
        user->linux_uid = linuxUid;
    }

    user->last_modified_by = subcall.user->id;

    UserQueries::updateUser(user);

    return ok("User updated successfully: " + user->name + "\n" + to_string(user));
}

void vh::shell::registerUserCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand("users", "List users",
                       [](const CommandCall& call) -> CommandResult {
                           if (!call.user) return invalid("You must be logged in to list users.");

                           if (!call.user->isAdmin() || !call.user->canManageUsers())
                               return invalid("You do not have permission to list users.");

                           return ok(to_string(UserQueries::listUsers()));
                       }, {});

    r->registerCommand("user", "Manage a single user",
                       [](const CommandCall& call) -> CommandResult {
                           if (call.positionals.empty()) return usage_user_root();

                           const std::string_view sub = call.positionals[0];
                           CommandCall subcall = call;
                           subcall.positionals.erase(subcall.positionals.begin());

                           if (sub == "create" || sub == "new") return createUser(subcall);
                           if (sub == "delete" || sub == "rm") return deleteUser(subcall);
                           if (sub == "info" || sub == "get") return handleUserInfo(subcall);
                           if (sub == "update" || sub == "set") return handleUpdateUser(subcall);

                           return invalid("Unknown user subcommand: '" + std::string(sub) + "'");
                       }, {"u"});
}

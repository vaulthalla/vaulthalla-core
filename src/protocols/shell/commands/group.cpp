#include "protocols/shell/commands.hpp"
#include "protocols/shell/Router.hpp"
#include "protocols/shell/types.hpp"
#include "util/shellArgsHelpers.hpp"
#include "database/Queries/GroupQueries.hpp"
#include "database/Queries/UserQueries.hpp"
#include "types/Group.hpp"
#include "types/User.hpp"
#include "auth/AuthManager.hpp"

using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using namespace vh::auth;

static CommandResult usage_group_root() {
    return {
        0,
        "Usage:\n"
        "  groups [list]\n"
        "  group create <name> [--desc <description>] [--linux-gid <id>]]\n"
        "  group delete <name> [--owner-id <id>>]\n"
        "  group info <name | id>\n"
        "  group update <name | id> [--name <new_name>] [--desc <description>] [--linux-gid <id>]\n"
        "  group add-user <group_name | gid> <user_name | uid>\n"
        "  group remove-user <group_name | gid> <user_name | uid>\n"
        "  group list-users <group_name | gid> [--owner-id <id>]\n",
        ""
    };
}

static CommandResult handle_group_create(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group create: you do not have permission to create groups");
    if (call.positionals.empty()) return invalid("group create: missing <name>");
    if (call.positionals.size() > 1) return invalid("group create: too many arguments");

    const std::string name = call.positionals[0];
    if (!AuthManager::isValidGroup(name)) return invalid("group create: invalid group name '" + name + "'");

    const auto group = std::make_shared<Group>();
    group->name = name;
    group->description = optVal(call, "desc").value_or("");

    if (const auto linuxGidOpt = optVal(call, "linux-gid")) {
        const auto parsed = parseInt(*linuxGidOpt);
        if (!parsed || *parsed <= 0) return invalid("group create: --linux-gid must be a positive integer");
        group->linux_gid = *parsed;
    }

    try {
        GroupQueries::createGroup(group);
        return ok("Successfully created new group:\n" + to_string(group));
    } catch (const std::exception& e) {
        return invalid("group create: " + std::string(e.what()));
    }
}

static CommandResult handle_group_update(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group update: you do not have permission to update groups");
    if (call.positionals.empty()) return invalid("group update: missing <name>");
    if (call.positionals.size() > 1) return invalid("group update: too many arguments");

    const std::string arg = call.positionals[0];
    const auto gidOpt = parseInt(arg);

    std::shared_ptr<Group> group;

    if (gidOpt) {
        if (*gidOpt <= 0) return invalid("group update: <id> must be a positive integer");
        group = GroupQueries::getGroup(*gidOpt);
    } else group = GroupQueries::getGroupByName(arg);

    if (!group) return invalid("group update: group '" + arg + "' not found");

    if (const auto newName = optVal(call, "name")) {
        if (!AuthManager::isValidGroup(*newName)) return invalid("group update: invalid group name '" + *newName + "'");
        group->name = *newName;
    }

    if (const auto desc = optVal(call, "desc")) group->description = *desc;

    if (const auto linuxGid = optVal(call, "linux-gid")) {
        const auto parsed = parseInt(*linuxGid);
        if (!parsed || *parsed <= 0) return invalid("group update: --linux-gid must be a positive integer");
        group->linux_gid = *parsed;
    }

    try {
        GroupQueries::updateGroup(group);
        return ok("Successfully updated group:\n" + to_string(group));
    } catch (const std::exception& e) {
        return invalid("group update: " + std::string(e.what()));
    }
}

static CommandResult handle_group_delete(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group delete: you do not have permission to delete groups");
    if (call.positionals.empty()) return invalid("group delete: missing <name>");
    if (call.positionals.size() > 1) return invalid("group delete: too many arguments");

    const std::string arg = call.positionals[0];
    const auto gidOpt = parseInt(arg);

    std::shared_ptr<Group> group;

    if (gidOpt) {
        if (*gidOpt <= 0) return invalid("group delete: <id> must be a positive integer");
        group = GroupQueries::getGroup(*gidOpt);
    } else group = GroupQueries::getGroupByName(arg);

    if (!group) return invalid("group delete: group '" + arg + "' not found");

    GroupQueries::deleteGroup(group->id);
    return ok("Successfully deleted group '" + group->name + "' (ID: " + std::to_string(group->id) + ")");
}

static CommandResult handle_group_info(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group info: you do not have permission to view group information");
    if (call.positionals.empty()) return invalid("group info: missing <name>");
    if (call.positionals.size() > 1) return invalid("group info: too many arguments");

    const auto arg = call.positionals[0];
    const auto gidOpt = parseInt(arg);

    std::shared_ptr<Group> group;

    if (gidOpt) {
        if (*gidOpt <= 0) return invalid("group info: <id> must be a positive integer");
        group = GroupQueries::getGroup(*gidOpt);
    } else group = GroupQueries::getGroupByName(arg);

    if (!group) return invalid("group info: group '" + arg + "' not found");

    return ok(to_string(group));
}

static CommandResult handle_group_list(const CommandCall& call) {
    int limit = 100; // Default limit
    if (auto lim = optVal(call, "limit")) {
        if (lim->empty()) return invalid("group list: --limit requires a value");
        auto parsed = parseInt(*lim);
        if (!parsed || *parsed <= 0) return invalid("group list: --limit must be a positive integer");
        limit = *parsed;
    }

    const auto groups = GroupQueries::listGroups();
    return ok(to_string(groups));
}

static CommandResult handle_group_add_user(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group add-user: you do not have permission to add users to groups");
    if (call.positionals.size() != 2) return invalid("group add-user: usage: group add-user <group_name> <user_name>");

    const std::string groupArg = call.positionals[0];
    const auto groupIdOpt = parseInt(groupArg);

    const std::string userArg = call.positionals[1];
    const auto userIdOpt = parseInt(userArg);

    std::shared_ptr<Group> group;

    if (groupIdOpt) {
        if (*groupIdOpt <= 0) return invalid("group add-user: <id> must be a positive integer");
        group = GroupQueries::getGroup(*groupIdOpt);
    } else group = GroupQueries::getGroupByName(groupArg);

    if (!group) return invalid("group add-user: group '" + groupArg + "' not found");

    std::shared_ptr<User> user;

    if (userIdOpt) {
        if (*userIdOpt <= 0) return invalid("group add-user: <id> must be a positive integer");
        user = UserQueries::getUserById(*userIdOpt);
    } else user = UserQueries::getUserByName(userArg);

    if (!user) return invalid("group add-user: user '" + userArg + "' not found");

    GroupQueries::addMemberToGroup(group->id, user->id);

    return ok("Successfully added user '" + user->name + "' to group '" + group->name + "'");
}

static CommandResult handle_group_remove_user(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group remove-user: you do not have permission to remove users from groups");
    if (call.positionals.size() != 2) return invalid("group remove-user: usage: group remove-user <group_name> <user_name>");

    const auto groupArg = call.positionals[0];
    const auto groupIdOpt = parseInt(groupArg);

    const auto userArg = call.positionals[1];
    const auto userIdOpt = parseInt(userArg);

    std::shared_ptr<Group> group;
    if (groupIdOpt) {
        if (*groupIdOpt <= 0) return invalid("group remove-user: <id> must be a positive integer");
        group = GroupQueries::getGroup(*groupIdOpt);
    } else group = GroupQueries::getGroupByName(groupArg);

    if (!group) return invalid("group remove-user: group '" + groupArg + "' not found");

    std::shared_ptr<User> user;

    if (userIdOpt) {
        if (*userIdOpt <= 0) return invalid("group remove-user: <id> must be a positive integer");
        user = UserQueries::getUserById(*userIdOpt);
    } else user = UserQueries::getUserByName(userArg);

    if (!user) return invalid("group remove-user: user '" + userArg + "' not found");

    GroupQueries::removeMemberFromGroup(group->id, user->id);

    return ok("Successfully removed user '" + user->name + "' from group '" + group->name + "'");
}

static CommandResult handle_group_list_users(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group list-users: you do not have permission to view group users");
    if (call.positionals.empty()) return invalid("group list-users: missing <group_name>");
    if (call.positionals.size() > 1) return invalid("group list-users: too many arguments");

    const std::string groupArg = call.positionals[0];
    const auto groupIdOpt = parseInt(groupArg);

    std::shared_ptr<Group> group;

    if (groupIdOpt) {
        if (*groupIdOpt <= 0) return invalid("group list-users: <id> must be a positive integer");
        group = GroupQueries::getGroup(*groupIdOpt);
    } else group = GroupQueries::getGroupByName(groupArg);

    if (!group) return invalid("group list-users: group '" + groupArg + "' not found");

    return ok(to_string(group->members));
}

void vh::shell::registerGroupCommands(const std::shared_ptr<Router>& r) {
    r->registerCommand("group", "Manage a single group",
                       [](const CommandCall& call) -> CommandResult {
                           if (call.positionals.empty()) return usage_group_root();

                           const std::string_view sub = call.positionals[0];
                           CommandCall subcall = call;
                           subcall.positionals.erase(subcall.positionals.begin());

                           if (sub == "create") return handle_group_create(subcall);
                           if (sub == "delete") return handle_group_delete(subcall);
                           if (sub == "info") return handle_group_info(subcall);
                           if (sub == "update") return handle_group_update(subcall);
                           if (sub == "add-user") return handle_group_add_user(subcall);
                           if (sub == "remove-user") return handle_group_remove_user(subcall);
                           if (sub == "list-users") return handle_group_list_users(subcall);

                           return invalid("group: unknown subcommand '" + std::string(sub) + "'");
                       }, {"g"});

    r->registerCommand("groups", "List groups",
                       [](const CommandCall& call) -> CommandResult {
                           return handle_group_list(call);
                       }, {"lg"});
}

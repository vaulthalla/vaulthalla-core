#include "protocols/shell/commands/all.hpp"
#include "protocols/shell/commands/helpers.hpp"
#include "protocols/shell/Router.hpp"
#include "protocols/shell/types.hpp"
#include "util/shellArgsHelpers.hpp"
#include "database/Queries/GroupQueries.hpp"
#include "identities/model/Group.hpp"
#include "identities/model/User.hpp"
#include "auth/AuthManager.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "usage/include/UsageManager.hpp"
#include "CommandUsage.hpp"

using namespace vh::shell;
using namespace vh::identities::model;
using namespace vh::database;
using namespace vh::auth;
using namespace vh::util;
using namespace vh::services;

static std::shared_ptr<Group> resolveGroup(const std::string& groupNameOrId) {
    if (const auto gidOpt = parseUInt(groupNameOrId)) {
        if (*gidOpt <= 0) throw std::runtime_error("Group ID must be a positive integer");
        return GroupQueries::getGroup(*gidOpt);
    }
    return GroupQueries::getGroupByName(groupNameOrId);
}

static void assignGidIfAvailable(const CommandCall& call, const std::shared_ptr<Group>& group, const std::shared_ptr<CommandUsage>& usage) {
    if (const auto linuxGidOpt = optVal(call, usage->resolveOptional("linux-gid")->option_tokens)) {
        const auto parsed = parseUInt(*linuxGidOpt);
        if (!parsed || *parsed <= 0) throw std::runtime_error("group create: --linux-gid must be a positive integer");
        group->linux_gid = *parsed;
    }
}

static CommandResult handle_group_create(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group create: you do not have permission to create groups");

    const auto usage = resolveUsage({"group", "create"});
    validatePositionals(call, usage);

    const std::string name = call.positionals[0];
    if (!AuthManager::isValidGroup(name)) return invalid("group create: invalid group name '" + name + "'");

    const auto group = std::make_shared<Group>();
    group->name = name;
    group->description = optVal(call, usage->resolveOptional("description")->option_tokens).value_or("");

    assignGidIfAvailable(call, group, usage);

    group->id = GroupQueries::createGroup(group);
    return ok("Successfully created new group:\n" + to_string(group));
}

static CommandResult handle_group_update(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group update: you do not have permission to update groups");

    const auto usage = resolveUsage({"group", "update"});
    validatePositionals(call, usage);

    const auto group = resolveGroup(call.positionals[0]);

    if (const auto newName = optVal(call, usage->resolveOptional("name")->option_tokens)) {
        if (!AuthManager::isValidGroup(*newName)) return invalid("group update: invalid group name '" + *newName + "'");
        group->name = *newName;
    }

    if (const auto desc = optVal(call, usage->resolveOptional("description")->option_tokens))
        group->description = *desc;

    assignGidIfAvailable(call, group, usage);

    GroupQueries::updateGroup(group);
    return ok("Successfully updated group:\n" + to_string(group));
}

static CommandResult handle_group_delete(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group delete: you do not have permission to delete groups");
    const auto usage = resolveUsage({"group", "delete"});
    validatePositionals(call, usage);
    const auto group = resolveGroup(call.positionals[0]);
    GroupQueries::deleteGroup(group->id);
    return ok("Successfully deleted group '" + group->name + "' (ID: " + std::to_string(group->id) + ")");
}

static CommandResult handle_group_info(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group info: you do not have permission to view group information");
    const auto usage = resolveUsage({"group", "info"});
    validatePositionals(call, usage);
    const auto group = resolveGroup(call.positionals[0]);
    return ok(to_string(group));
}

static CommandResult handle_group_list(const CommandCall& call) {
    const auto usage = resolveUsage({"group", "list"});
    validatePositionals(call, usage);

    auto params = parseListQuery(call);

    std::vector<std::shared_ptr<Group>> groups;
    if (!call.user->canManageGroups()) groups = GroupQueries::listGroups(call.user->id, std::move(params));
    else groups = GroupQueries::listGroups(std::nullopt, std::move(params));

    return ok(to_string(groups));
}

static CommandResult handle_group_add_user(const CommandCall& call) {
    constexpr const auto* ERR = "group add user";

    if (!call.user->canManageGroups()) return invalid("group add-user: you do not have permission to add users to groups");

    const auto usage = resolveUsage({"group", "user", "add"});
    validatePositionals(call, usage);

    const auto group = resolveGroup(call.positionals[0]);

    const auto uLkp = resolveUser(call.positionals[1], ERR);
    if (!uLkp || !uLkp.ptr) return invalid(uLkp.error);
    const auto user = uLkp.ptr;

    GroupQueries::addMemberToGroup(group->id, user->id);

    return ok("Successfully added user '" + user->name + "' to group '" + group->name + "'");
}

static CommandResult handle_group_remove_user(const CommandCall& call) {
    constexpr const auto* ERR = "group remove user";

    if (!call.user->canManageGroups()) return invalid("group remove-user: you do not have permission to remove users from groups");

    const auto usage = resolveUsage({"group", "user", "remove"});
    validatePositionals(call, usage);

    const auto group = resolveGroup(call.positionals[0]);

    const auto uLkp = resolveUser(call.positionals[1], ERR);
    if (!uLkp || !uLkp.ptr) return invalid(uLkp.error);
    const auto user = uLkp.ptr;

    GroupQueries::removeMemberFromGroup(group->id, user->id);

    return ok("Successfully removed user '" + user->name + "' from group '" + group->name + "'");
}

static CommandResult handle_group_list_users(const CommandCall& call) {
    if (!call.user->canManageGroups()) return invalid("group list-users: you do not have permission to view group users");
    const auto usage = resolveUsage({"group", "list-users"});
    const auto group = resolveGroup(call.positionals[0]);
    return ok(to_string(group->members));
}

static bool isGroupUserMatch(const std::string& cmd, const std::string_view input) {
    return isCommandMatch({"group", "user", cmd}, input);
}

static CommandResult handle_group_user(const CommandCall& call) {
    const auto [action, subcall] = descend(call);
    if (isGroupUserMatch("add", action)) return handle_group_add_user(subcall);
    if (isGroupUserMatch("remove", action)) return handle_group_remove_user(subcall);
    if (isGroupUserMatch("list", action)) return handle_group_list_users(subcall);
    return invalid(call.constructFullArgs(), "Unknown group user action: '" + std::string(action) + "'");
}

static bool isGroupMatch(const std::string& cmd, const std::string_view input) {
    return isCommandMatch({"group", cmd}, input);
}

static CommandResult handle_group(const CommandCall& call) {
    if (call.positionals.empty() || hasKey(call, "help") || hasKey(call, "h"))
        return usage(call.constructFullArgs());

    const auto [sub, subcall] = descend(call);

    if (isGroupMatch("create", sub)) return handle_group_create(subcall);
    if (isGroupMatch("update", sub)) return handle_group_update(subcall);
    if (isGroupMatch("delete", sub)) return handle_group_delete(subcall);
    if (isGroupMatch("info", sub)) return handle_group_info(subcall);
    if (isGroupMatch("list", sub)) return handle_group_list(subcall);
    if (isGroupMatch("user", sub)) return handle_group_user(subcall);

    return invalid(call.constructFullArgs(), "Unknown group subcommand: '" + std::string(sub) + "'");
}

void commands::registerGroupCommands(const std::shared_ptr<Router>& r) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;
    r->registerCommand(usageManager->resolve("group"), handle_group);
}

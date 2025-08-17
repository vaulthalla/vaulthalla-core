// #include "protocols/shell/commands.hpp"
// #include "protocols/shell/Router.hpp"
// #include "util/shellArgsHelpers.hpp"
// #include "database/Queries/GroupQueries.hpp"
// #include "types/Group.hpp"
// #include "types/User.hpp"
//
// using namespace vh::shell;
// using namespace vh::types;
// using namespace vh::database;
//
// // ---------- pretty/usage ----------
// static CommandResult usage_group_root() {
//     return {
//         0,
//         "Usage:\n"
//         "  group create <name> [--desc <description>] [--owner-id <id>]\n"
//         "  group delete <name> [--owner-id <id>>]\n"
//         "  group info <name> [--owner-id <id>]\n"
//         "  group list [--owner-id <id>] [--limit <n>]\n"
//         "  group update <name> [--desc <description>] [--owner-id <id>]\n"
//         "  group add-user <group_name> <user_name>\n"
//         "  group remove-user <group_name> <user_name>\n"
//         "  group list-users <group_name> [--owner-id <id>]\n
//         "  group list-groups [--owner-id <id>] [--limit <n>]\n",
//         ""
//     };
// }
//
// static CommandResult handle_group_create(const CommandCall& call) {
//     if (call.positionals.empty()) return invalid("group create: missing <name>");
//     if (call.positionals.size() > 1) return invalid("group create: too many arguments");
//
//     const std::string name = call.positionals[0];
//     if (!AuthManager::isValidName(name)) return invalid("group create: invalid group name '" + name + "'");
//
//     auto group = std::make_shared<Group>();
//     group->name = name;
//     group->description = optVal(call, "desc").value_or("");
//     group->owner_id = optVal(call, "owner-id").has_value() ? parseInt(optVal(call, "owner-id").value()) : call.user->id;
//
//     if (group->owner_id <= 0) return invalid("group create: --owner-id must be a positive integer");
//
//     try {
//         GroupQueries::createGroup(group);
//         return ok("Successfully created new group: " + to_string(group));
//     } catch (const std::exception& e) {
//         return invalid("group create: " + std::string(e.what()));
//     }
// }
//
// static CommandResult handle_group_delete(const CommandCall& call) {
//     if (call.positionals.empty()) return invalid("group delete: missing <name>");
//     if (call.positionals.size() > 1) return invalid("group delete: too many arguments");
//
//     const std::string name = call.positionals[0];
//     const auto ownerIdOpt = optVal(call, "owner-id");
//     unsigned int ownerId = call.user->id;
//
//     if (ownerIdOpt) {
//         const auto parsed = parseInt(*ownerIdOpt);
//         if (!parsed || *parsed <= 0) return invalid("group delete: --owner-id must be a positive integer");
//         ownerId = *parsed;
//     }
//
//     auto group = GroupQueries::getGroup(name, ownerId);
//     if (!group) return invalid("group delete: group '" + name + "' not found for user ID " + std::to_string(ownerId));
//
//     if (!call.user->canManageGroups() && group->owner_id != call.user->id)
//         return invalid("group delete: you do not have permission to delete this group");
//
//     GroupQueries::deleteGroup(group->id);
//     return ok("Successfully deleted group '" + group->name + "' (ID: " + std::to_string(group->id) + ")");
// }
//
// static CommandResult handle_group_info(const CommandCall& call) {
//     if (call.positionals.empty()) return invalid("group info: missing <name>");
//     if (call.positionals.size() > 1) return invalid("group info: too many arguments");
//
//     const std::string name = call.positionals[0];
//     const auto ownerIdOpt = optVal(call, "owner-id");
//     unsigned int ownerId = call.user->id;
//
//     if (ownerIdOpt) {
//         const auto parsed = parseInt(*ownerIdOpt);
//         if (!parsed || *parsed <= 0) return invalid("group info: --owner-id must be a positive integer");
//         ownerId = *parsed;
//     }
//
//     auto group = GroupQueries::getGroup(name, ownerId);
//     if (!group) return invalid("group info: group '" + name + "' not found for user ID " + std::to_string(ownerId));
//
//     if (!call.user->canManageGroups() && group->owner_id != call.user->id)
//         return invalid("group info: you do not have permission to view this group");
//
//     return ok(to_string(group));
// }
//
// static CommandResult handle_group_list(const CommandCall& call) {
//     const auto ownerIdOpt = optVal(call, "owner-id");
//     unsigned int ownerId = call.user->id;
//
//     if (ownerIdOpt) {
//         const auto parsed = parseInt(*ownerIdOpt);
//         if (!parsed || *parsed <= 0) return invalid("group list: --owner-id must be a positive integer");
//         ownerId = *parsed;
//     }
//
//     int limit = 100; // Default limit
//     if (auto lim = optVal(call, "limit")) {
//         if (lim->empty()) return invalid("group list: --limit requires a value");
//         auto parsed = parseInt(*lim);
//         if (!parsed || *parsed <= 0) return invalid("group list: --limit must be a positive integer");
//         limit = *parsed;
//     }
//
//     const auto groups = GroupQueries::listGroups(ownerId, limit);
//     return ok(to_string(groups));
// }
//
// static CommandResult handle_group_add_user(const CommandCall& call) {
//     if (call.positionals.size() != 2) return invalid("group add-user: usage: group add-user <group_name> <user_name>");
//
//     const std::string groupName = call.positionals[0];
//     const std::string userName = call.positionals[1];
//
//     auto group = GroupQueries::getGroup(groupName, call.user->id);
//     if (!group) return invalid("group add-user: group '" + groupName + "' not found");
//
//     if (!call.user->canManageGroups() && group->owner_id != call.user->id)
//         return invalid("group add-user: you do not have permission to modify this group");
//
//     auto user = UserQueries::getUserByName(userName);
//     if (!user) return invalid("group add-user: user '" + userName + "' not found");
//
//     GroupQueries::addUserToGroup(group->id, user->id);
//     return ok("Successfully added user '" + user->name + "' to group '" + group->name + "'");
// }
//
// static CommandResult handle_group_remove_user(const CommandCall& call) {
//     if (call.positionals.size() != 2) return invalid("group remove-user: usage: group remove-user <group_name> <user_name>");
//
//     const std::string groupName = call.positionals[0];
//     const std::string userName = call.positionals[1];
//
//     auto group = GroupQueries::getGroup(groupName, call.user->id);
//     if (!group) return invalid("group remove-user: group '" + groupName + "' not found");
//
//     if (!call.user->canManageGroups() && group->owner_id != call.user->id)
//         return invalid("group remove-user: you do not have permission to modify this group");
//
//     auto user = UserQueries::getUserByName(userName);
//     if (!user) return invalid("group remove-user: user '" + userName + "' not found");
//
//     GroupQueries::removeUserFromGroup(group->id, user->id);
//     return ok("Successfully removed user '" + user->name + "' from group '" + group->name + "'");
// }
//
// static CommandResult handle_group_list_users(const CommandCall& call) {
//     if (call.positionals.empty()) return invalid("group list-users: missing <group_name>");
//     if (call.positionals.size() > 1) return invalid("group list-users: too many arguments");
//
//     const std::string groupName = call.positionals[0];
//     auto group = GroupQueries::getGroup(groupName, call.user->id);
//     if (!group) return invalid("group list-users: group '" + groupName + "' not found");
//
//     if (!call.user->canManageGroups() && group->owner_id != call.user->id)
//         return invalid("group list-users: you do not have permission to view this group");
//
//     auto users = GroupQueries::listUsersInGroup(group->id);
//     return ok(to_string(users));
// }
//
// static CommandResult handle_group_list_groups(const CommandCall& call) {
//     const auto ownerIdOpt = optVal(call, "owner-id");
//     unsigned int ownerId = call.user->id;
//
//     if (ownerIdOpt) {
//         const auto parsed = parseInt(*ownerIdOpt);
//         if (!parsed || *parsed <= 0) return invalid("group list-groups: --owner-id must be a positive integer");
//         ownerId = *parsed;
//     }
//
//     int limit = 100; // Default limit
//     if (auto lim = optVal(call, "limit")) {
//         if (lim->empty()) return invalid("group list-groups: --limit requires a value");
//         auto parsed = parseInt(*lim);
//         if (!parsed || *parsed <= 0) return invalid("group list-groups: --limit must be a positive integer");
//         limit = *parsed;
//     }
//
//     const auto groups = GroupQueries::listGroups(ownerId, limit);
//     return ok(to_string(groups));
// }
//
// void vh::shell::registerGroupCommands(const std::shared_ptr<Router>& r) {
//     r->registerCommand("group", "Manage a single group",
//                        [](const CommandCall& call) -> CommandResult {
//                            if (call.positionals.empty()) return usage_group_root();
//
//                            const std::string_view sub = call.positionals[0];
//                            CommandCall subcall = call;
//                            subcall.positionals.erase(subcall.positionals.begin());
//
//                            if (sub == "create") return handle_group_create(subcall);
//                            if (sub == "delete") return handle_group_delete(subcall);
//                            if (sub == "info") return handle_group_info(subcall);
//                            if (sub == "update") return handle_group_create(subcall); // Reusing create logic for update
//                            if (sub == "add-user") return handle_group_add_user(subcall);
//                            if (sub == "remove-user") return handle_group_remove_user(subcall);
//                            if (sub == "list-users") return handle_group_list_users(subcall);
//
//                            return invalid("group: unknown subcommand '" + std::string(sub) + "'");
//                        }, {"g"});
//
//     r->registerCommand("groups", "List groups",
//                        [](const CommandCall& call) -> CommandResult {
//                            return handle_group_list_groups(call);
//                        }, {"ls"});
// }

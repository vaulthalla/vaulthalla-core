#include "GroupUsage.hpp"

using namespace vh::shell;

CommandBook GroupUsage::all() {
    CommandBook book;
    book.title = "Vaulthalla Group Commands";
    book.commands = {
        group(),
        groups_list(),
        group_create(),
        group_delete(),
        group_info(),
        group_update(),
        group_user(),
        group_list_users()
    };
    return book;
}

CommandUsage GroupUsage::groups_list() {
    CommandUsage cmd;
    cmd.ns = "groups";
    cmd.description = "List all groups in the system.";
    cmd.optional = {
        {"--limit <number>", "Limit the number of groups returned (default 100)"}
    };
    cmd.examples.push_back({"vh groups", "List all groups."});
    return cmd;
}

CommandUsage GroupUsage::group() {
    auto cmd = buildBaseUsage_();
    cmd.description = "Manage a single group.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (create, delete, info, update, user, list-users)"}};
    cmd.examples.push_back({"vh group create devs --desc \"Development Team\" --linux-gid 1001", "Create a new group named 'devs'."});
    cmd.examples.push_back({"vh group delete devs", "Delete the group named 'devs'."});
    cmd.examples.push_back({"vh group info devs", "Show information for the group named 'devs'."});
    cmd.examples.push_back({"vh group update devs --name developers --desc \"Updated Description\" --linux-gid 1002", "Update the 'devs' group with a new name, description, and Linux GID."});
    cmd.examples.push_back({"vh group user add devs alice", "Add user 'alice' to the 'devs' group."});
    cmd.examples.push_back({"vh group user remove devs alice", "Remove user 'alice' from the 'devs' group."});
    cmd.examples.push_back({"vh group list-users devs", "List all users in the 'devs' group."});
    return cmd;
}

CommandUsage GroupUsage::group_create() {
    auto cmd = buildBaseUsage_();
    cmd.command = "create";
    cmd.command_aliases = {"new", "add", "mk"};
    cmd.description = "Create a new group.";
    cmd.positionals = {{"<name>", "Name of the new group"}};
    cmd.optional = {
        {"--desc <description>", "Optional description for the group"},
        {"--linux-gid <id>", "Optional Linux GID for system integration"}
    };
    cmd.examples.push_back({"vh group create devs --desc \"Development Team\" --linux-gid 1001", "Create a new group named 'devs' with description and Linux GID."});
    return cmd;
}

CommandUsage GroupUsage::group_delete() {
    auto cmd = buildBaseUsage_();
    cmd.command = "delete";
    cmd.command_aliases = {"remove", "del", "rm"};
    cmd.description = "Delete an existing group by name or ID.";
    cmd.positionals = {{"<name|id>", "Name or ID of the group to delete"}};
    cmd.examples.push_back({"vh group delete devs", "Delete the group named 'devs'."});
    cmd.examples.push_back({"vh group rm 42", "Delete the group with ID 42 (using alias)."});
    return cmd;
}

CommandUsage GroupUsage::group_info() {
    auto cmd = buildBaseUsage_();
    cmd.command = "info";
    cmd.command_aliases = {"show", "get"};
    cmd.description = "Display detailed information about a group.";
    cmd.positionals = {{"<name|id>", "Name or ID of the group"}};
    cmd.examples.push_back({"vh group info devs", "Show information for the group named 'devs'."});
    cmd.examples.push_back({"vh group show 42", "Show information for the group with ID 42 (using alias)."});
    return cmd;
}

CommandUsage GroupUsage::group_update() {
    auto cmd = buildBaseUsage_();
    cmd.command = "update";
    cmd.command_aliases = {"set", "modify", "edit"};
    cmd.description = "Update properties of an existing group.";
    cmd.positionals = {{"<name|id>", "Name or ID of the group to update"}};
    cmd.optional = {
        {"--name <new_name>", "New name for the group"},
        {"--desc <description>", "New description for the group"},
        {"--linux-gid <id>", "New Linux GID for system integration"}
    };
    cmd.examples.push_back({"vh group update devs --name developers --desc \"Updated Description\" --linux-gid 1002", "Update the 'devs' group with a new name, description, and Linux GID."});
    return cmd;
}

CommandUsage GroupUsage::group_user() {
    auto cmd = buildBaseUsage_();
    cmd.command = "user";
    cmd.command_aliases = {"u"};
    cmd.description = "Add or remove users from a group.";
    cmd.positionals = {{"<add|remove>", "Action to perform"}, {"<group_name|gid>", "Name or ID of the group"}, {"<user_name|uid>", "Username or ID of the user"}};
    cmd.examples.push_back({"vh group user add devs alice", "Add user 'alice' to the 'devs' group."});
    cmd.examples.push_back({"vh group user remove 42 1001", "Remove user with ID 1001 from the group with ID 42."});
    return cmd;
}

CommandUsage GroupUsage::group_list_users() {
    auto cmd = buildBaseUsage_();
    cmd.command = "users";
    cmd.description = "List all users in a specific group.";
    cmd.positionals = {{"<group_name|gid>", "Name or ID of the group"}};
    cmd.examples.push_back({"vh group users devs", "List all users in the 'devs' group."});
    return cmd;
}

CommandUsage GroupUsage::buildBaseUsage_() {
    CommandUsage cmd;
    cmd.ns = "group";
    cmd.ns_aliases = {"g"};
    return cmd;
}

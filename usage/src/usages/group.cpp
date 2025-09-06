#include "usages.hpp"

using namespace vh::shell;

namespace vh::shell::group {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    return cmd;
}

static std::shared_ptr<CommandUsage> list(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all groups in the system.";
    cmd->optional = {
        {"--limit <number>", "Limit the number of groups returned (default 100)"}
    };
    cmd->examples.push_back({"vh groups", "List all groups."});
    return cmd;
}

static std::shared_ptr<CommandUsage> create(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new group.";
    cmd->positionals = {{"group_name", "Name of the new group", {"name", {"name", "group_name"}}}};
    cmd->optional = {
        {"description", "Optional description for the group", {"desc", {"description"}}},
        {"linux_group", "Optional Linux GID for system integration", {"linux-gid", {"id"}}}
    };
    cmd->examples = {
        {"vh group create devs --desc \"Development Team\" --linux-gid 1001", "Create a new group named 'devs'."},
        {"vh group mk admins --linux-gid 2001", "Create a new group named 'admins' with Linux GID 2001 (using alias)."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> remove(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"delete", "remove", "del", "rm"};
    cmd->description = "Delete an existing group by name or ID.";
    cmd->positionals = {{"group", "Name or ID of the group to delete", {"name", "id"}}};
    cmd->examples = {
        {"vh group delete devs", "Delete the group named 'devs'."},
        {"vh group rm 42", "Delete the group with ID 42 (using alias)."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> info(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Display detailed information about a group.";
    cmd->positionals = {{"group", "Name or ID of the group", {"name", "id"}}};
    cmd->examples = {
        {"vh group info devs", "Show information for the group named 'devs'."},
        {"vh group get 42", "Show information for the group with ID 42 (using alias)."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> update(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "mod", "modify"};
    cmd->description = "Update properties of an existing group.";
    cmd->positionals = {{"group", "Name or ID of the group to update", {"name", "id"}}};
    cmd->optional = {
        {"group_name", "New name for the group", {"name", {"name", "group_name"}}},
        {"description", "New description for the group", {"desc", {"description"}}},
        {"linux_group", "New Linux GID for system integration", {"linux-gid", {"id"}}}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> user_list(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all users in a specific group.";
    cmd->positionals = {{"group", "Name or ID of the group", {"name", "id"}}};
    cmd->examples = {
        {"vh group users devs", "List all users in the 'devs' group."},
        {"vh group users 42", "List all users in the group with ID 42."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> user_add(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"add", "new", "mk"};
    cmd->description = "Add a user to a specific group.";
    cmd->positionals = {
        {"group", "Name or ID of the group", {"group", {"name", "id"}}},
        {"user", "Username or ID of the user to add", {"user", {"name", "id"}}}
    };
    cmd->examples = {
        {"vh group user add devs alice", "Add user 'alice' to the 'devs' group."},
        {"vh group user add 42 1001", "Add user with ID 1001 to the group with ID 42."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> user_remove(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"remove", "del", "rm"};
    cmd->description = "Remove a user from a specific group.";
    cmd->positionals = {
        {"<group_name|gid>", "Name or ID of the group"},
        {"<user_name|uid>", "Username or ID of the user"}
    };
    cmd->examples.push_back({"vh group user remove devs alice", "Remove user 'alice' from the 'devs' group."});
    cmd->examples.push_back({"vh group user remove 42 1001", "Remove user with ID 1001 from the group with ID 42."});
    return cmd;
}

static std::shared_ptr<CommandUsage> group_user(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"user", "u"};
    cmd->pluralAliasImpliesList = true;
    cmd->description = "Add or remove users from a group.";
    cmd->subcommands = {
        user_add(cmd->weak_from_this()),
        user_remove(cmd->weak_from_this()),
        user_list(cmd->weak_from_this())
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> base(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"group", "g"};
    cmd->pluralAliasImpliesList = true;
    cmd->description = "Manage a single group.";
    cmd->examples = {
        {"vh group create devs --desc \"Development Team\" --linux-gid 1001", "Create a new group named 'devs'."},
        {"vh group delete devs", "Delete the group named 'devs'."},
        {"vh group info devs", "Show information for the group named 'devs'."},
        {"vh group update devs --desc \"Updated Description\" --linux-gid 2001", "Update the description and Linux GID of 'devs'."},
        {"vh group user add devs alice", "Add user 'alice' to the 'devs' group."},
        {"vh group user remove devs alice", "Remove user 'alice' from the 'devs' group."},
        {"vh group users devs", "List all users in the 'devs' group."}
    };
    cmd->subcommands = {
        list(cmd->weak_from_this()),
        create(cmd->weak_from_this()),
        remove(cmd->weak_from_this()),
        info(cmd->weak_from_this()),
        update(cmd->weak_from_this()),
        group_user(cmd->weak_from_this())
    };
    return cmd;
}

std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent) {
    const auto book = std::make_shared<CommandBook>();
    book->title = "Group Commands";
    book->root = base(parent);
    return book;
}

}

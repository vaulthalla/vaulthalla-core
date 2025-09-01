#include "usages.hpp"

using namespace vh::shell;

namespace vh::shell::group {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    return cmd;
}

static std::shared_ptr<CommandUsage> list(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all groups in the system.";
    cmd->optional = {
        {"--limit <number>", "Limit the number of groups returned (default 100)"}
    };
    cmd->examples.push_back({"vh groups", "List all groups."});
    return cmd;
}

static std::shared_ptr<CommandUsage> create(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new group.";
    cmd->positionals = {{"<name>", "Name of the new group"}};
    cmd->optional = {
        {"--desc <description>", "Optional description for the group"},
        {"--linux-gid <id>", "Optional Linux GID for system integration"}
    };
    cmd->examples.push_back({"vh group create devs --desc \"Development Team\" --linux-gid 1001", "Create a new group named 'devs' with description and Linux GID."});
    return cmd;
}

static std::shared_ptr<CommandUsage> remove(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"delete", "remove", "del", "rm"};
    cmd->description = "Delete an existing group by name or ID.";
    cmd->positionals = {{"<name|id>", "Name or ID of the group to delete"}};
    cmd->examples.push_back({"vh group delete devs", "Delete the group named 'devs'."});
    cmd->examples.push_back({"vh group rm 42", "Delete the group with ID 42 (using alias)."});
    return cmd;
}

static std::shared_ptr<CommandUsage> info(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Display detailed information about a group.";
    cmd->positionals = {{"<name|id>", "Name or ID of the group"}};
    cmd->examples.push_back({"vh group info devs", "Show information for the group named 'devs'."});
    cmd->examples.push_back({"vh group show 42", "Show information for the group with ID 42 (using alias)."});
    return cmd;
}

static std::shared_ptr<CommandUsage> update(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "mod", "modify"};
    cmd->description = "Update properties of an existing group.";
    cmd->positionals = {{"<name|id>", "Name or ID of the group to update"}};
    cmd->optional = {
        {"--name <new_name>", "New name for the group"},
        {"--desc <description>", "New description for the group"},
        {"--linux-gid <id>", "New Linux GID for system integration"}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> user_list(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all users in a specific group.";
    cmd->positionals = {{"<group_name|gid>", "Name or ID of the group"}};
    cmd->examples.push_back({"vh group users devs", "List all users in the 'devs' group."});
    return cmd;
}

static std::shared_ptr<CommandUsage> user_add(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"add", "new", "mk"};
    cmd->description = "Add a user to a specific group.";
    cmd->positionals = {
        {"<group_name|gid>", "Name or ID of the group"},
        {"<user_name|uid>", "Username or ID of the user"}
    };
    cmd->examples.push_back({"vh group user add devs alice", "Add user 'alice' to the 'devs' group."});
    cmd->examples.push_back({"vh group user add 42 1001", "Add user with ID 1001 to the group with ID 42."});
    return cmd;
}

static std::shared_ptr<CommandUsage> user_remove(const std::shared_ptr<CommandUsage>& parent) {
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

static std::shared_ptr<CommandUsage> group_user(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"user", "u"};
    cmd->pluralAliasImpliesList = true;
    cmd->description = "Add or remove users from a group.";
    cmd->subcommands = {
        user_add(cmd->shared_from_this()),
        user_remove(cmd->shared_from_this()),
        user_list(cmd->shared_from_this())
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> base(const std::shared_ptr<CommandUsage>& parent) {
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
        list(cmd->shared_from_this()),
        create(cmd->shared_from_this()),
        remove(cmd->shared_from_this()),
        info(cmd->shared_from_this()),
        update(cmd->shared_from_this()),
        group_user(cmd->shared_from_this())
    };
    return cmd;
}

std::shared_ptr<CommandBook> get(const std::shared_ptr<CommandUsage>& parent) {
    const auto book = std::make_shared<CommandBook>();
    book->title = "Group Commands";
    book->root = base(parent);
    return book;
}

}

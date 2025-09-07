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
    cmd->optional_flags = {
        Flag::WithAliases("json_filter", "Output the list in JSON format", {"json", "j"})
    };
    cmd->optional = {
        Optional::Single("result_limit", "Limit the number of results returned (default 100)", "limit", "limit", "100"),
        Optional::Single("page_number", "Specify the page number when using --limit for pagination (default 1)", "page", "page", "1")
    };
    cmd->examples.push_back({"vh groups", "List all groups."});
    return cmd;
}

static std::shared_ptr<CommandUsage> create(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new group.";
    cmd->positionals = {
        Positional::Alias("group", "Name for the new group", "name")
    };
    cmd->optional = {
        Optional::Single("description", "The description of the new group.", "desc", "description"),
        Optional::Single("linux_group", "The Linux GID for system integration.", "linux-gid", "id")
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
    cmd->positionals = {
        Positional::WithAliases("group", "Name or ID of the group to delete", {"name", "id"})
    };
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
    cmd->positionals = {
        Positional::WithAliases("group", "Name or ID of the group", {"name", "id"})
    };
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
    cmd->positionals = {
        Positional::WithAliases("group", "Name or ID of the group to update", {"name", "id"})
    };
    cmd->optional = {
        Optional::Single("description", "The description of the new group.", "desc", "description"),
        Optional::Single("linux_group", "The Linux GID for system integration.", "linux-gid", "id"),
        Optional::Mirrored("group_name", "The new name for the group.", "name")
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> user_list(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all users in a specific group.";
    cmd->positionals = {
        Positional::WithAliases("group", "Name or ID of the group", {"name", "id"})
    };
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
        Positional::WithAliases("group", "Name or ID of the group", {"name", "id"}),
        Positional::WithAliases("user", "Username or ID of the user to add", {"name", "id"})
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
        Positional::WithAliases("group", "Name or ID of the group", {"name", "id"}),
        Positional::WithAliases("user", "Username or ID of the user to remove", {"name", "id"})
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

    // ---------- build subcommands (stack pointers) ----------
    const auto listCmd        = list(cmd->weak_from_this());
    const auto createCmd      = create(cmd->weak_from_this());
    const auto removeCmd      = remove(cmd->weak_from_this());
    const auto infoCmd        = info(cmd->weak_from_this());
    const auto updateCmd      = update(cmd->weak_from_this());

    // membership subtree
    const auto groupUserCmd   = group_user(cmd->weak_from_this());
    const auto usersCmd       = user_list(groupUserCmd->weak_from_this());
    const auto userAddCmd     = user_add(groupUserCmd->weak_from_this());
    const auto userRmCmd      = user_remove(groupUserCmd->weak_from_this());

    // ---------- test lifecycles ----------
    // list: should tolerate empty; best with a few groups present
    listCmd->test_usage = {
        .setup    = { TestCommandUsage::Multiple(createCmd) },
        .teardown = { TestCommandUsage::Multiple(removeCmd, 0, 0) }
    };

    // create: verify info/update/users, then clean
    createCmd->test_usage = {
        .lifecycle = {
            TestCommandUsage::Single(infoCmd),
            TestCommandUsage::Single(updateCmd),
            TestCommandUsage::Single(usersCmd),
            // add/remove a user to ensure membership paths don’t regress
            TestCommandUsage::Single(userAddCmd),
            TestCommandUsage::Single(userRmCmd)
        },
        .teardown = { TestCommandUsage::Single(removeCmd) }
    };

    // remove: ensure a group exists first
    removeCmd->test_usage = {
        .setup = { TestCommandUsage::Single(createCmd) }
    };

    // info: exercise on fresh groups; clean them up
    infoCmd->test_usage = {
        .setup    = { TestCommandUsage::Multiple(createCmd) },
        .teardown = { TestCommandUsage::Multiple(removeCmd) }
    };

    // update: same as info; validate round-trips don’t corrupt invariants
    updateCmd->test_usage = {
        .setup    = { TestCommandUsage::Multiple(createCmd) },
        .teardown = { TestCommandUsage::Multiple(removeCmd) }
    };

    // users (list members): make sure members exist, then remove them, then delete group
    usersCmd->test_usage = {
        .setup = {
            TestCommandUsage::Single(createCmd),
            TestCommandUsage::Multiple(userAddCmd) // add 1..3 members
        },
        .teardown = {
            TestCommandUsage::Multiple(userRmCmd), // remove all we added
            TestCommandUsage::Single(removeCmd)
        }
    };

    // membership add: ensure group exists; list users afterward; clean up
    userAddCmd->test_usage = {
        .setup    = { TestCommandUsage::Single(createCmd) },
        .lifecycle = { TestCommandUsage::Single(usersCmd) },
        .teardown = {
            TestCommandUsage::Single(userRmCmd),
            TestCommandUsage::Single(removeCmd)
        }
    };

    // membership remove: ensure group + at least one member
    userRmCmd->test_usage = {
        .setup = {
            TestCommandUsage::Single(createCmd),
            TestCommandUsage::Single(userAddCmd)
        },
        .lifecycle = { TestCommandUsage::Single(usersCmd) },
        .teardown = { TestCommandUsage::Single(removeCmd) }
    };

    // ---------- finalize ----------
    cmd->subcommands = {
        listCmd,
        createCmd,
        removeCmd,
        infoCmd,
        updateCmd,
        usersCmd,      // expose "vh group users" as sibling
        groupUserCmd   // expose "vh group user [add|remove]"
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

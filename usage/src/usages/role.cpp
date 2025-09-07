#include "usages.hpp"

using namespace vh::shell;

namespace vh::shell::role {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    return cmd;
}

static std::shared_ptr<CommandUsage> list(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all roles in the system.";
    cmd->optional_flags = {
        Flag::WithAliases("json_filter", "Output the list in JSON format", {"json", "j"})
    };
    cmd->optional_flags = {
        Flag::Alias("user_filer", "List only user roles", "user"),
        Flag::Alias("vault_filter", "List only vault roles", "vault")
    };
    cmd->optional = {
        Optional::Single("json_filter", "Output the list in JSON format", "json", "j")
    };
    cmd->examples.push_back({"vh roles", "List all roles."});
    cmd->examples.push_back({"vh roles --user --json", "List all user roles in JSON format."});
    return cmd;
}

static std::shared_ptr<CommandUsage> info(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Display detailed information about a specific role.";
    cmd->positionals = {
        {"role", "ID or name of the role", {"id", "name"}}
    };
    cmd->optional_flags = {
        Flag::WithAliases("user_filter", "Indicates the role is a user role", {"user", "u"}),
        Flag::WithAliases("vault_filter", "Indicates the role is a vault role", {"vault", "v"})
    };
    cmd->examples.push_back({"vh role info 42", "Show information for the role with ID 42."});
    cmd->examples.push_back({"vh role info admin --user", "Show information for the user role named 'admin'."});
    return cmd;
}

static std::shared_ptr<CommandUsage> create(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new role with specified permissions.";
    cmd->positionals = {Positional::Alias("role_name", "Name of the new role", "name")};
    cmd->required = {
        Option::OneToMany("role_type", "Type of the role", "type", {"user", "vault"})
    };
    cmd->optional_flags = {
        Flag::WithAliases("permissions_flags", "Permission flags to set for the new role (see 'vh permissions')", ALL_SHELL_PERMS_STR)
    };
    cmd->optional = {
        Optional::OneToMany("inherit_perms", "Inherit permissions from an existing role ID", "from", {"id", "name"}),
    };
    cmd->examples.push_back({"vh role create editor --type user --set-manage-users --set-manage-groups",
                           "Create a new user role named 'editor' with user and group management permissions."});
    cmd->examples.push_back({"vh role create vault-admin --type vault --from 3 --set-manage-access",
                           "Create a new vault role named 'vault-admin' by copying permissions from role ID 3 and adding manage access permission."});
    return cmd;
}

static std::shared_ptr<CommandUsage> remove(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"delete", "remove", "del", "rm"};
    cmd->description = "Delete an existing role by ID.";
    cmd->positionals = {{"role_id", "ID of the role to delete", {"id"}}};
    cmd->examples.push_back({"vh role delete 42", "Delete the role with ID 42."});
    cmd->examples.push_back({"vh role rm 42", "Delete the role with ID 42 (using alias)."});
    return cmd;
}

static std::shared_ptr<CommandUsage> update(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "modify", "edit"};
    cmd->description = "Update properties and permissions of an existing role.";
    cmd->positionals = {{"<id>", "ID of the role to update"}};
    cmd->optional_flags = {
        Flag::WithAliases("permissions_flags", "Permission flags to set or unset for the role (see 'vh permissions')", ALL_SHELL_PERMS_STR)
    };
    cmd->optional = {
        Optional::Single("role_name", "New name for the role", "name", "new_name"),
    };
    cmd->examples.push_back({"vh role update 42 --name superadmin --set-manage-admins",
                           "Rename role ID 42 to 'superadmin' and add admin management permission."});
    cmd->examples.push_back({"vh role update 3 --unset-manage-users",
                           "Remove user management permission from role ID 3."});
    cmd->examples.push_back({"vh role update 5 --set-manage-access --set-manage-tags",
                           "Add manage access and manage tags permissions to vault role ID 5."});

    return cmd;
}

static std::shared_ptr<CommandUsage> base(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"role", "r"};
    cmd->pluralAliasImpliesList = true;
    cmd->description = "Manage a single role.";

    // ---------- build subcommands ----------
    const auto listCmd   = list(cmd->weak_from_this());
    const auto createCmd = create(cmd->weak_from_this());
    const auto removeCmd = remove(cmd->weak_from_this());
    const auto infoCmd   = info(cmd->weak_from_this());
    const auto updateCmd = update(cmd->weak_from_this());

    // ---------- test lifecycles ----------
    // list: should work even with no roles, but better to create some first
    listCmd->test_usage = {
        .setup    = { TestCommandUsage::Multiple(createCmd) },
        .teardown = { TestCommandUsage::Multiple(removeCmd, 0, 0) }
    };

    // create: after creation, verify info/update paths, then delete
    createCmd->test_usage = {
        .lifecycle = {
            TestCommandUsage::Single(infoCmd),
            TestCommandUsage::Single(updateCmd)
        },
        .teardown = { TestCommandUsage::Single(removeCmd) }
    };

    // remove: must ensure a role exists first
    removeCmd->test_usage = {
        .setup = { TestCommandUsage::Single(createCmd) }
    };

    // info: exercise against a fresh role; then clean up
    infoCmd->test_usage = {
        .setup    = { TestCommandUsage::Multiple(createCmd) },
        .teardown = { TestCommandUsage::Multiple(removeCmd) }
    };

    // update: same deal as info
    updateCmd->test_usage = {
        .setup    = { TestCommandUsage::Multiple(createCmd) },
        .teardown = { TestCommandUsage::Multiple(removeCmd) }
    };

    // ---------- finalize ----------
    cmd->subcommands = { listCmd, createCmd, removeCmd, infoCmd, updateCmd };
    return cmd;
}

std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent) {
    const auto book = std::make_shared<CommandBook>();
    book->title = "Role Commands";
    book->root = base(parent);
    return book;
}

}

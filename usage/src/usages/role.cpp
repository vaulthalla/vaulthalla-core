#include "usages.hpp"

using namespace vh::shell;

namespace vh::shell::role {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    return cmd;
}

static std::shared_ptr<CommandUsage> list(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all roles in the system.";
    cmd->optional = {
        {"--user", "List only user roles"},
        {"--vault", "List only vault roles"},
        {"--json", "Output the list in JSON format"},
        {"--limit <n>", "Limit the number of results returned"}
    };
    cmd->examples.push_back({"vh roles", "List all roles."});
    cmd->examples.push_back({"vh roles --user --json", "List all user roles in JSON format."});
    return cmd;
}

static std::shared_ptr<CommandUsage> info(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Display detailed information about a specific role.";
    cmd->positionals = {{"<id|name>", "ID or name of the role"}};
    cmd->optional = {
        {"--user", "Specify if the role is a user role when using name"},
        {"--vault", "Specify if the role is a vault role when using name"}
    };
    cmd->examples.push_back({"vh role info 42", "Show information for the role with ID 42."});
    cmd->examples.push_back({"vh role info admin --user", "Show information for the user role named 'admin'."});
    return cmd;
}

static std::shared_ptr<CommandUsage> create(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new role with specified permissions.";
    cmd->positionals = {{"<name>", "Name for the new role"}};
    cmd->required = {
        {"--type <user|vault>", "Type of role to create (user or vault)"}
    };
    cmd->optional = {
        {"--from <id|name>", "ID or name of an existing role to copy permissions from"},
        {"<permission_flags>", "Permission flags to set for the new role (see 'vh help role create')"}
    };
    cmd->examples.push_back({"vh role create editor --type user --set-manage-users --set-manage-groups",
                           "Create a new user role named 'editor' with user and group management permissions."});
    cmd->examples.push_back({"vh role create vault-admin --type vault --from 3 --set-manage-access",
                           "Create a new vault role named 'vault-admin' by copying permissions from role ID 3 and adding manage access permission."});
    return cmd;
}

static std::shared_ptr<CommandUsage> remove(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"delete", "remove", "del", "rm"};
    cmd->description = "Delete an existing role by ID.";
    cmd->positionals = {{"<id>", "ID of the role to delete"}};
    cmd->examples.push_back({"vh role delete 42", "Delete the role with ID 42."});
    cmd->examples.push_back({"vh role rm 42", "Delete the role with ID 42 (using alias)."});
    return cmd;
}

static std::shared_ptr<CommandUsage> update(const std::shared_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "modify", "edit"};
    cmd->description = "Update properties and permissions of an existing role.";
    cmd->positionals = {{"<id>", "ID of the role to update"}};
    cmd->optional = {
        {"--name <new_name>", "New name for the role"},
        {"<permission_flag>", "Permission flags to add or remove (see 'vh help role update')"}
    };
    cmd->examples.push_back({"vh role update 42 --name superadmin --set-manage-admins",
                           "Rename role ID 42 to 'superadmin' and add admin management permission."});
    cmd->examples.push_back({"vh role update 3 --unset-manage-users",
                           "Remove user management permission from role ID 3."});
    cmd->examples.push_back({"vh role update 5 --set-manage-access --set-manage-tags",
                           "Add manage access and manage tags permissions to vault role ID 5."});

    return cmd;
}

static std::shared_ptr<CommandUsage> base(const std::shared_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->description = "Manage a single role.";
    cmd->subcommands = {
        list(cmd->shared_from_this()),
        create(cmd->shared_from_this()),
        remove(cmd->shared_from_this()),
        info(cmd->shared_from_this()),
        update(cmd->shared_from_this())
    };
    return cmd;
}

std::shared_ptr<CommandBook> get(const std::shared_ptr<CommandUsage>& parent) {
    const auto book = std::make_shared<CommandBook>();
    book->title = "Role Commands";
    book->root = base(parent);
    return book;
}

}

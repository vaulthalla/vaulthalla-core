#include "usages.hpp"

using namespace vh::shell;

namespace vh::shell::role {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    return cmd;
}

static const auto userFlag = Flag::WithAliases("user_filter", "Indicates the role is a user role", {"user", "u"});
static const auto vaultFlag = Flag::WithAliases("vault_filter", "Indicates the role is a vault role", {"vault", "v"});
static const auto rolePos = Positional::WithAliases("role", "ID or name of the role", {"id", "name"});
static const auto roleType = Option::OneToMany("role_type", "Type of the role", "type", {"user", "vault"});
static const auto inheritFrom = Optional::OneToMany("inherit_from", "Inherit permissions from an existing role ID", "from", {"id", "name"});
static const auto roleNameOpt = Optional::Single("role_name", "New name for the role", "name", "new_name");
static const auto newRolePos = Positional::Alias("role_name", "Name for the new role", "name");
static const auto typePos = Positional::WithAliases("role_type", "Type of the role (user | vault)", {"user", "vault"});
static const auto descriptionOpt = Optional::ManyToOne("description", "Description of the role", {"description", "desc", "d"}, "description");

static std::shared_ptr<CommandUsage> list(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all roles in the system.";
    cmd->optional_flags = { jsonFlag, userFlag, vaultFlag };
    cmd->examples.push_back({"vh roles", "List all roles."});
    cmd->examples.push_back({"vh roles --user --json", "List all user roles in JSON format."});
    return cmd;
}

static std::shared_ptr<CommandUsage> info(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Display detailed information about a specific role.";
    cmd->positionals = { rolePos };
    cmd->examples.push_back({"vh role info 42", "Show information for the role with ID 42."});
    cmd->examples.push_back({"vh role info admin --user", "Show information for the user role named 'admin'."});
    return cmd;
}

static std::shared_ptr<CommandUsage> create(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new role with specified permissions.";
    cmd->positionals = { typePos, newRolePos };
    cmd->optional_flags = { permissionsFlags };
    cmd->optional = { descriptionOpt, inheritFrom };
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
    cmd->positionals = { rolePos };
    cmd->examples.push_back({"vh role delete 42", "Delete the role with ID 42."});
    cmd->examples.push_back({"vh role rm 42", "Delete the role with ID 42 (using alias)."});
    return cmd;
}

static std::shared_ptr<CommandUsage> update(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "modify", "edit"};
    cmd->description = "Update properties and permissions of an existing role.";
    cmd->positionals = { rolePos };
    cmd->optional_flags = { permissionsFlags };
    cmd->optional = { descriptionOpt, roleNameOpt };
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

    const auto createSingle = TestCommandUsage::Single(createCmd);
    const auto createMultiple = TestCommandUsage::Multiple(createCmd);
    const auto removeSingle = TestCommandUsage::Single(removeCmd);
    const auto removeMultiple = TestCommandUsage::Multiple(removeCmd);
    const auto infoSingle = TestCommandUsage::Single(infoCmd);
    const auto updateSingle = TestCommandUsage::Single(updateCmd);

    // ---------- test lifecycles ----------

    listCmd->test_usage.setup = { createMultiple };
    listCmd->test_usage.teardown = { removeMultiple };

    createCmd->test_usage.lifecycle = { infoSingle, updateSingle };
    createCmd->test_usage.teardown = { removeSingle };

    removeCmd->test_usage.setup = { createSingle };

    infoCmd->test_usage.setup = { createMultiple };
    infoCmd->test_usage.teardown = { removeMultiple };

    updateCmd->test_usage.setup = { createMultiple };
    updateCmd->test_usage.teardown = { removeMultiple };

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

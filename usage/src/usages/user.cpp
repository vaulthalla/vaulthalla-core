#include "usages.hpp"

using namespace vh::shell;

namespace vh::shell::user {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    return cmd;
}

static std::shared_ptr<CommandUsage> list(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all users in the system.";
    cmd->examples = {
        {"vh users", "List all users in the system."},
        {"vh user", "List all users in the system (using alias)."},
        {"vh u", "List all users in the system (using shortest alias)."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> create(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new user.";
    cmd->required = {
        Option::Same("name", "Username for the new user"),
        Option::Same("role", "Role name or ID for the new user")
    };
    cmd->optional = {
        Optional::Same("email", "Email address of the new user"),
        Optional::Single("linux_uid", "Linux UID for system integration", "linux-uid", "uid")
    };
    cmd->examples = {
        {"vh user create --name alice --role admin --email alice123@icann.org --linux-uid 1001",
         "Create a new user named 'alice' with admin role, email, and Linux UID."},
        {"vh user new --name bob --role user --email bon@icann.org --linux-uid 1002",
         "Create a new user named 'bob' with user role, email, and Linux UID (using alias)."},
        {"vh u mk --name charlie --role 2", "Create a new user named 'charlie' with role ID 2 (using shortest alias)."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> remove(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"delete", "remove", "rm"};
    cmd->description = "Delete an existing user by username.";
    cmd->positionals = {
        Positional::Alias("username", "Username of the user to delete", "name")
    };
    cmd->examples = {
        {"vh user delete alice", "Delete the user named 'alice'."},
        {"vh user remove bob", "Delete the user named 'bob' (using alias)."},
        {"vh u rm charlie", "Delete the user named 'charlie' (using shortest alias)."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> info(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Get information about a specific user by username.";
    cmd->positionals = {
        Positional::Alias("username", "Username of the user to get information about", "name")
    };
    cmd->examples = {
        {"vh user info alice", "Get information about the user named 'alice'."},
        {"vh user get bob", "Get information about the user named 'bob' (using alias)."},
        {"vh u show charlie", "Get information about the user named 'charlie' (using shortest alias)."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> update(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "modify", "edit"};
    cmd->description = "Update properties of an existing user.";
    cmd->positionals = {{"<name>", "Username of the user to update"}};
    cmd->optional = {
        Optional::Single("username", "New username", "name", "new_name"),
        Optional::Single("email", "New email address", "email", "new_email"),
        Optional::Single("role", "New role name or ID", "role", "new_role"),
        Optional::Single("linux_uid", "New Linux UID", "linux-uid", "new_linux_uid")
    };
    cmd->examples = {
        {"vh user update alice --email alice123@icann.org --role user",
         "Update user 'alice' with a new email and role."},
        {"vh user set bob --name robert --linux-uid 2002",
         "Change username of 'bob' to 'robert' and update Linux UID (using alias)."},
        {"vh u edit charlie --email charlie@limewire.net --role 3",
         "Update user 'charlie' with a new email and role ID (using shortest alias)."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> base(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"user", "u"};
    cmd->description = "Manage a single user.";
    cmd->pluralAliasImpliesList = true;
    cmd->subcommands = {
        list(cmd->weak_from_this()),
        create(cmd->weak_from_this()),
        remove(cmd->weak_from_this()),
        info(cmd->weak_from_this()),
        update(cmd->weak_from_this())
    };
    cmd->examples = {
        {"vh user create --name alice --role admin --email alice123@icann.org --linux-uid 1001",
         "Create a new user named 'alice' with admin role, email, and Linux UID."},
        {"vh user delete alice", "Delete the user named 'alice'."},
        {"vh user info alice", "Get information about the user named 'alice'."},
        {"vh user update alice --email alice123@icann.org --role user",
         "Update user 'alice' with a new email and role."}
    };
    return cmd;
}

std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent) {
    const auto book = std::make_shared<CommandBook>();
    book->title = "User Commands";
    book->root = base(parent);
    return book;
}

}

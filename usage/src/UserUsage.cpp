#include "UserUsage.hpp"

using namespace vh::shell;

CommandBook UserUsage::all() {
    CommandBook book;
    book.title = "Vaulthalla User Commands";
    book.commands = {
        user(),
        users_list(),
        user_create(),
        user_delete(),
        user_info(),
        user_update()
    };
    return book;
}

CommandUsage UserUsage::users_list() {
    CommandUsage cmd;
    cmd.ns = "users";
    cmd.ns_aliases = {"user", "u"};
    cmd.command = "[list]";
    cmd.description = "List all users in the system.";
    cmd.examples = {
        {"vh users", "List all users in the system."},
        {"vh user", "List all users in the system (using alias)."},
        {"vh u", "List all users in the system (using shortest alias)."}
    };
    return cmd;
}

CommandUsage UserUsage::user() {
    auto cmd = buildBaseUsage_();
    cmd.description = "Manage a single user.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (create, delete, info, update)"}};
    cmd.examples = {
        {"vh user create --name alice --role admin --email alice123@icann.org --linux-uid 1001",
         "Create a new user named 'alice' with admin role, email, and Linux UID."},
        {"vh user delete alice", "Delete the user named 'alice'."},
        {"vh user info alice", "Get information about the user named 'alice'."},
        {"vh user update alice --email alice123@icann.org --role user",
         "Update user 'alice' with a new email and role."}
    };
    return cmd;
}

CommandUsage UserUsage::user_create() {
    auto cmd = buildBaseUsage_();
    cmd.command = "create";
    cmd.command_aliases = {"new", "add", "mk"};
    cmd.description = "Create a new user.";
    cmd.required = {
        {"--name <name>", "Username for the new user"},
        {"--role <role>", "Role name or ID for the new user"}
    };
    cmd.optional = {
        {"--email <email>", "Email address of the new user"},
        {"--linux-uid <uid>", "Linux UID for system integration"}
    };
    cmd.examples = {
        {"vh user create --name alice --role admin --email alice123@icann.org --linux-uid 1001",
         "Create a new user named 'alice' with admin role, email, and Linux UID."},
        {"vh user new --name bob --role user --email bon@icann.org --linux-uid 1002",
         "Create a new user named 'bob' with user role, email, and Linux UID (using alias)."},
        {"vh u mk --name charlie --role 2", "Create a new user named 'charlie' with role ID 2 (using shortest alias)."}
    };
    return cmd;
}

CommandUsage UserUsage::user_delete() {
    auto cmd = buildBaseUsage_();
    cmd.command = "delete";
    cmd.command_aliases = {"remove", "del", "rm"};
    cmd.description = "Delete an existing user by username.";
    cmd.positionals = {{"<name>", "Username of the user to delete"}};
    cmd.examples = {
        {"vh user delete alice", "Delete the user named 'alice'."},
        {"vh user remove bob", "Delete the user named 'bob' (using alias)."},
        {"vh u rm charlie", "Delete the user named 'charlie' (using shortest alias)."}
    };
    return cmd;
}

CommandUsage UserUsage::user_info() {
    auto cmd = buildBaseUsage_();
    cmd.command = "info";
    cmd.command_aliases = {"get", "show"};
    cmd.description = "Get information about a specific user by username.";
    cmd.positionals = {{"<name>", "Username of the user to retrieve"}};
    cmd.examples = {
        {"vh user info alice", "Get information about the user named 'alice'."},
        {"vh user get bob", "Get information about the user named 'bob' (using alias)."},
        {"vh u show charlie", "Get information about the user named 'charlie' (using shortest alias)."}
    };
    return cmd;
}

CommandUsage UserUsage::user_update() {
    auto cmd = buildBaseUsage_();
    cmd.command = "update";
    cmd.command_aliases = {"set", "modify", "edit"};
    cmd.description = "Update properties of an existing user.";
    cmd.positionals = {{"<name>", "Username of the user to update"}};
    cmd.optional = {
        {"--name <new_name>", "New username"},
        {"--email <email>", "New email address"},
        {"--role <role>", "New role name or ID"},
        {"--linux-uid <uid>", "New Linux UID"}
    };
    cmd.examples = {
        {"vh user update alice --email alice123@icann.org --role user",
         "Update user 'alice' with a new email and role."},
        {"vh user set bob --name robert --linux-uid 2002",
         "Change username of 'bob' to 'robert' and update Linux UID (using alias)."},
        {"vh u edit charlie --email charlie@limewire.net --role 3",
         "Update user 'charlie' with a new email and role ID (using shortest alias)."}
    };
    return cmd;
}

CommandUsage UserUsage::buildBaseUsage_() {
    CommandUsage cmd;
    cmd.ns = "user";
    cmd.ns_aliases = {"u"};
    return cmd;
}


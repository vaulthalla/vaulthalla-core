#include "protocols/shell/usage/UserUsage.hpp"

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
    cmd.examples.push_back({"vh users", "List all users."});
    cmd.examples.push_back({"vh user list", "List all users (using alias)."});
    cmd.examples.push_back({"vh u list", "List all users (using shortest alias)."});
    return cmd;
}

CommandUsage UserUsage::user() {
    auto cmd = buildBaseUsage_();
    cmd.description = "Manage a single user.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (create, delete, info, update)"}};
    cmd.examples.push_back({"vh user create --name alice --role admin --email test123@gmail.com --linux-uid 1001",
                           "Create a new user named 'alice' with admin role, email, and Linux UID."});
    cmd.examples.push_back({"vh user delete alice", "Delete the user named 'alice'."});
    cmd.examples.push_back({"vh user info alice", "Get information about the user named 'alice'."});
    cmd.examples.push_back({"vh user update alice --email alice123@gmail --role user",
                           "Update user 'alice' with a new email and role."});
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
    cmd.examples.push_back({"vh user create --name alice --role admin --email test123@gmail.com --linux-uid 1001",
                           "Create a new user named 'alice' with admin role, email, and Linux UID."});
    cmd.examples.push_back({"vh user new --name bob --role 2", "Create a new user named 'bob' with role ID 2."});
    cmd.examples.push_back({"vh u mk --name charlie --role user", "Create a new user named 'charlie' with 'user' role (using shortest alias)."});
    return cmd;
}

CommandUsage UserUsage::user_delete() {
    auto cmd = buildBaseUsage_();
    cmd.command = "delete";
    cmd.command_aliases = {"remove", "del", "rm"};
    cmd.description = "Delete an existing user by username.";
    cmd.positionals = {{"<name>", "Username of the user to delete"}};
    cmd.examples.push_back({"vh user delete alice", "Delete the user named 'alice'."});
    cmd.examples.push_back({"vh user rm bob", "Delete the user named 'bob' (using alias)."});
    cmd.examples.push_back({"vh u del charlie", "Delete the user named 'charlie' (using shortest alias)."});
    return cmd;
}

CommandUsage UserUsage::user_info() {
    auto cmd = buildBaseUsage_();
    cmd.command = "info";
    cmd.command_aliases = {"get", "show"};
    cmd.description = "Get information about a specific user by username.";
    cmd.positionals = {{"<name>", "Username of the user to retrieve"}};
    cmd.examples.push_back({"vh user info alice", "Get information about the user named 'alice'."});
    cmd.examples.push_back({"vh user get bob", "Get information about the user named 'bob' (using alias)."});
    cmd.examples.push_back({"vh u show charlie", "Get information about the user named 'charlie' (using shortest alias)."});
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
    cmd.examples.push_back({"vh user update alice --email alice123@gmail.com --role user",
                           "Update user 'alice' with a new email and role."});
    cmd.examples.push_back({"vh user set bob --name robert --linux-uid 2002",
                           "Update user 'bob' to 'robert' with a new Linux UID (using alias)."});
    cmd.examples.push_back({"vh u edit charlie --role 3", "Update user 'charlie' to role ID 3 (using shortest alias)."});
    return cmd;
}

CommandUsage UserUsage::buildBaseUsage_() {
    CommandUsage cmd;
    cmd.ns = "user";
    cmd.ns_aliases = {"u"};
    return cmd;
}


#include "protocols/shell/usage/VaultUsage.hpp"
#include "protocols/shell/CommandUsage.hpp"

using namespace vh::shell;

CommandBook VaultUsage::all() {
    CommandBook book;
    book.title = "Vaulthalla Vault Commands";
    book.commands = {
        vault(),
        vaults_list(),
        vault_create(),
        vault_delete(),
        vault_info(),
        vault_update(),
        vault_role(),
        vault_keys(),
        vault_sync()
    };
    return book;
}

CommandUsage VaultUsage::vault() {
    auto cmd = buildBaseUsage_();
    cmd.description = "Manage a single vault.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (create, delete, info, update, role, keys, sync)"}};
    cmd.examples = {
        {"vh vault create myvault --local", "Create a new local vault named 'myvault'."},
        {"vh vault delete 42", "Delete the vault with ID 42."},
        {"vh vault info myvault --owner alice", "Show information about the vault named 'myvault' owned by user 'alice'."},
        {"vh vault update 42 --desc \"New Description\"", "Update the description of the vault with ID 42."},
        {"vh vault role 42 add bob --role read", "Add user 'bob' with 'read' role to the vault with ID 42."},
        {"vh vault keys rotate 42", "Rotate the encryption keys for the vault with ID 42."},
        {"vh vault sync 42", "Initiate a sync operation for the vault with ID 42."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_create() {
    auto cmd = buildBaseUsage_();
    cmd.command = "create";
    cmd.command_aliases = {"new", "add", "mk"};
    cmd.description = "Create a new vault. Supports local and S3-backed vaults.";
    cmd.positionals = {{"<name>", "Name of the new vault"}};
    cmd.required = {{"--local | --l | --s3  | --s", "Type of vault to create (local or S3)"}};
    cmd.optional = {
        {"--desc <description>", "Optional description for the vault"},
        {"--quota <size|unlimited>", "Optional storage quota (e.g. 10G, 500M). Default is unlimited."},
        {"--owner <id|name>", "User ID or username of the vault owner. Default is the current user."},
    };

    cmd.groups.push_back({"Local Vault Options", {
        {"--on-conflict <overwrite | keep_both | ask>",
         "Conflict resolution strategy for local vaults. Default is 'overwrite'."}
    }});

    cmd.groups.push_back({"S3 Vault Options", {
        {"--api-key <name | id>", "Name or ID of the API key to access the S3 bucket"},
        {"--bucket <name>", "Name of the S3 bucket"},
        {"--strategy <cache | sync | mirror>", "Sync strategy for S3 vaults. Default is 'cache'."},
        {"--on-conflict <keep_local|keep_remote|ask>",
         "Conflict resolution strategy during sync. Default is 'ask'."}
    }});

    cmd.examples = {
        {"vh vault create mylocalvault --local --desc \"My Local Vault\" --quota 10G", "Create a local vault with a 10GB quota."},
        {"vh vault create mys3vault --s3 --api-key mykey --bucket mybucket --strategy sync --on-conflict keep_local",
         "Create an S3-backed vault with specified API key, bucket, sync strategy, and conflict resolution."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_delete() {
    auto cmd = buildBaseUsage_();
    cmd.command = "delete";
    cmd.command_aliases = {"remove", "del", "rm"};
    cmd.description = "Delete an existing vault by ID or name.";
    cmd.positionals = {{"<id | name>", "ID or name of the vault to delete"}};
    cmd.optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd.examples = {
        {"vh vault delete 42", "Delete the vault with ID 42."},
        {"vh vault delete myvault --owner alice", "Delete the vault named 'myvault' owned by user 'alice'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_info() {
    auto cmd = buildBaseUsage_();
    cmd.command = "info";
    cmd.command_aliases = {"show", "get"};
    cmd.description = "Display detailed information about a vault.";
    cmd.positionals = {{"<id | name>", "ID or name of the vault"}};
    cmd.optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd.examples = {
        {"vh vault info 42", "Show information for the vault with ID 42."},
        {"vh vault info myvault --owner alice", "Show information for the vault named 'myvault' owned by user 'alice'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vaults_list() {
    CommandUsage cmd;
    cmd.ns = "vaults";
    cmd.ns_aliases = {"vault"};
    cmd.optional = {{"list | ls", "If call 'vault' is used, 'list' or 'ls' must be provided as a positional argument"}};
    cmd.description = "List all vaults accessible to the current user.";
    cmd.optional = {
        {"--local", "Show only local vaults"},
        {"--s3", "Show only S3-backed vaults"},
        {"--limit <n>", "Limit the number of results to n vaults"},
    };
    cmd.examples = {
        {"vh vaults", "List all vaults accessible to the current user."},
        {"vh vaults --local", "List only local vaults."},
        {"vh vaults --s3 --limit 5", "List up to 5 S3-backed vaults."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_update() {
    auto cmd = buildBaseUsage_();
    cmd.command = "update";
    cmd.command_aliases = {"set", "modify", "edit"};
    cmd.description = "Update properties of an existing vault.";
    cmd.positionals = {{"<id|name>", "ID or name of the vault to update"}};
    cmd.optional = {
        {"--desc <description>", "New description for the vault"},
        {"--quota <size|unlimited>", "New storage quota (e.g. 10G, 500M). Use 'unlimited' to remove quota."},
        {"--owner <id|name>", "New owner user ID or username"},
        {"--api-key <name|id>", "New API key name or ID for S3 vaults"},
        {"--bucket <name>", "New S3 bucket name for S3 vaults"},
        {"--sync-strategy <cache|sync|mirror>", "New sync strategy for S3 vaults"},
        {"--on-sync-conflict <overwrite|keep_both|ask|keep_local|keep_remote>",
         "New conflict resolution strategy"},
    };
    cmd.examples = {
        {"vh vault update 42 --desc \"Updated Description\" --quota 20G", "Update the description and quota of the vault with ID 42."},
        {"vh vault update myvault --owner bob", "Change the owner of the vault named 'myvault' to user 'bob'."},
        {"vh vault update mys3vault --api-key newkey --bucket newbucket --sync-strategy mirror",
         "Update the API key, bucket, and sync strategy of the S3-backed vault named 'mys3vault'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_role() {
    auto cmd = buildBaseUsage_();
    cmd.command = "role";
    cmd.command_aliases = {"r"};
    cmd.description = "Manage vault roles and permissions.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (assign, override)"}};
    cmd.required = {{"<id | name>", "ID or name of the vault"}};
    cmd.optional = {
        {"--owner <id | name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd.groups = {
        {
            "Role Assignment",
            {
                "assign <role_id> --[uid|gid|user|group] <id|name>",
                "Assign a role to a user or group by UID, GID, username, or group name"
            }
        },
        {
            "Permission Overrides",
            {
                "override <role_id> --[uid|gid|user|group] <id|name> <--permission_flag> --[allow|deny] [--pattern <regex>] [--enabled <bool=true>]",
                "Override a specific permission for a user or group"
            }
        }
    };

    return cmd;
}

CommandUsage VaultUsage::vault_keys() {
    auto cmd = buildBaseUsage_();
    cmd.command = "keys";
    cmd.command_aliases = {"k"};
    cmd.description = "Manage API keys for accessing S3-backed vaults.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (list, create, delete)"}};
    cmd.examples = {
        {"vh vault keys 42", "List all API keys associated with the vault ID 42."},
        {"vh vault keys myvault --owner alice", "List all API keys for the vault named 'myvault' owned by user 'alice'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_sync() {
    auto cmd = buildBaseUsage_();
    cmd.command = "sync";
    cmd.command_aliases = {"s"};
    cmd.description = "Manage vault synchronization settings and operations.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (set, info, update)"}};
    cmd.examples = {
        {"vh vault sync info 42", "Show sync configuration for the vault with ID 42."},
        {"vh vault sync set myvault --strategy sync --on-conflict keep_local --api-key mykey --bucket mybucket",
         "Set up sync for the vault named 'myvault' with specified strategy, conflict resolution, API key, and bucket."}
    };
    return cmd;
}

CommandUsage VaultUsage::buildBaseUsage_() {
    CommandUsage cmd;
    cmd.ns = "vault";
    cmd.ns_aliases = {"v"};
    return cmd;
}

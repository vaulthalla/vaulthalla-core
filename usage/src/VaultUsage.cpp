#include "VaultUsage.hpp"
#include "CommandUsage.hpp"

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
        vault_role_assign(),
        vault_role_override(),
        vault_role_remove(),
        vault_role_list(),
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
        {"vh vault create myvault --local --desc \"My Local Vault\" --quota 10G", "Create a local vault with a 10GB quota."},
        {"vh vault delete myvault --owner alice", "Delete the vault named 'myvault' owned by user 'alice'."},
        {"vh vault info myvault --owner alice", "Show information for the vault named 'myvault' owned by user 'alice'."},
        {"vh vault update myvault --desc \"Updated Description\" --quota 20G", "Update the description and quota of 'myvault'."},
        {"vh vault role myvault --add bob --perm download,upload", "Add user 'bob' with download and upload permissions to 'myvault'."},
        {"vh vault keys myvault --list", "List all encryption keys associated with 'myvault'."},
        {"vh vault sync myvault", "Manually trigger a sync for 'myvault'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_create() {
    auto cmd = buildBaseUsage_();
    cmd.command = "create";
    cmd.command_aliases = {"new", "add", "mk"};
    cmd.description = "Create a new vault. Supports local and S3-backed vaults.";
    cmd.positionals = {{"<name>", "Name of the new vault"}};
    cmd.required = {{"--local | --s3", "Type of vault to create (local or S3)"}};
    cmd.optional = {
        {"--interactive", "Run in interactive mode, prompting for missing information"},
        {"--desc <description>", "Optional description for the vault"},
        {"--quota <size|unlimited>", "Optional storage quota (e.g. 10G, 500M). Default is unlimited."},
        {"--owner <id|name>", "User ID or username of the vault owner. Default is the current user."},
    };
    cmd.groups = {
        {"Local Vault Options", {
            {"--local", "Create a local vault (mutually exclusive with --s3)"},
            {"--on-sync-conflict <overwrite | keep_both | ask>",
             "Conflict resolution strategy for local vaults. Default is 'overwrite'."}
        }},
        {"S3 Vault Options", {
            {"--s3", "Create an S3-backed vault (mutually exclusive with --local)"},
            {"--api-key <name | id>", "Name or ID of the API key to access the S3 bucket"},
            {"--bucket <name>", "Name of the S3 bucket"},
            {"--sync-strategy <cache | sync | mirror>", "Sync strategy for S3 vaults. Default is 'cache'."},
            {"--on-sync-conflict <keep_local | keep_remote | ask>", "Conflict resolution strategy during sync. Default is 'ask'."},
            {"--encrypt", "Enable upstream encryption for S3 vaults. This is the default."},
            {"--no-encrypt", "Disable upstream encryption for S3 vaults."},
            {"--accept-overwrite-waiver", "Acknowledge the risks of enabling encryption on an upstream s3 bucket with existing files."},
            {"--accept-decryption-waiver", "Acknowledge the risks of disabling encryption on an upstream s3 bucket with existing encrypted files."}
        }}
    };
    cmd.examples = {
        {"vh vault create myvault --local --desc \"My Local Vault\" --quota 10G", "Create a local vault with a 10GB quota."},
        {"vh vault create s3vault --s3 --api-key myapikey --bucket mybucket", "Create an S3-backed vault."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_delete() {
    auto cmd = buildBaseUsage_();
    cmd.command = "delete";
    cmd.command_aliases = {"remove", "del", "rm"};
    cmd.description = "Delete an existing vault by ID or name.";
    cmd.positionals = {{"<id|name>", "ID or name of the vault to delete"}};
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
    cmd.positionals = {{"<id|name>", "ID or name of the vault"}};
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
    cmd.ns_aliases = {"vault", "vls", "v-list", "vault ls"};
    cmd.command = "[list]";
    cmd.command_aliases = {"list", "ls"};
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
        {"--on-sync-conflict <overwrite|keep_both|ask|keep_local|keep_remote>", "New conflict resolution strategy"},
        {"--encrypt", "Enable upstream encryption for S3 vaults. This is the default."},
        {"--no-encrypt", "Disable upstream encryption for S3 vaults."},
        {"--accept-overwrite-waiver", "Acknowledge the risks of enabling encryption on an upstream s3 bucket with existing files."},
        {"--accept-decryption-waiver", "Acknowledge the risks of disabling encryption on an upstream s3 bucket with existing encrypted files."}
    };
    cmd.examples = {
        {"vh vault update 42 --desc \"Updated Description\" --quota 20G", "Update the description and quota of the vault with ID 42."},
        {"vh vault update myvault --owner bob --api-key newkey --bucket newbucket --sync-strategy mirror --on-sync-conflict keep_remote --owner alice",
         "Update multiple properties of the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_role_assign() {
    auto cmd = buildBaseUsage_();
    cmd.command = "role assign";
    cmd.command_aliases = {"r a", "role add", "r add"};
    cmd.description = "Assign a role to a user or group for a specific vault.";
    cmd.positionals = {{"<vault-id|vault-name>", "ID or name of the vault"}, {"<role_id>", "ID of the role to assign"}};
    cmd.required = {{"--uid | --gid | --user | --group", "Specify the user or group to assign the role to"}};
    cmd.optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd.examples = {
        {"vh vault role assign 42 read-only bob", "Add user 'bob' to the 'read-only' role for the vault with ID 42."},
        {"vh vault role assign myvault read-write developers --owner alice", "Add group 'developers' to the 'read-write' role for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_role_remove_assignment() {
    auto cmd = buildBaseUsage_();
    cmd.command = "role unassign";
    cmd.command_aliases = {"r ua", "role ua", "r unassign", "role remove", "r rm", "role rm"};
    cmd.description = "Remove a role assignment from a user or group for a specific vault.";
    cmd.positionals = {{"<vault-id|vault-name>", "ID or name of the vault"}, {"<role_id>", "ID of the role to unassign"}};
    cmd.required = {{"--uid | --gid | --user | --group", "Specify the user or group to remove the role from"}};
    cmd.optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd.examples = {
        {"vh vault role unassign 42 read-only bob", "Remove user 'bob' from the 'read-only' role for the vault with ID 42."},
        {"vh vault role unassign myvault read-write developers --owner alice", "Remove group 'developers' from the 'read-write' role for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_role_add_override() {
    auto cmd = buildBaseUsage_();
    cmd.command = "role override add";
    cmd.command_aliases = {"r oa", "role oa", "r override add", "role add-override", "role override new", "r override new"};
    cmd.description = "Add a permission override for a user or group in a specific vault role.";
    cmd.positionals = {{"<vault-id|vault-name>", "ID or name of the vault"}, {"<role_id>", "ID of the role to override"}};
    cmd.required = {
        {"--uid | --gid | --user | --group", "Specify the user or group to override the permission for"},
        {"<--permission_flag>", "Permission flag to override (e.g. --download, --upload, --delete, etc.)"},
        {"--allow | --deny", "Specify whether to allow or deny the permission"}
    };
    cmd.optional = {
        {"--pattern <regex>", "Optional regex pattern to scope the override to specific paths"},
        {"--effect", "If set, the pattern is treated as a positive match; otherwise, it's a negative match"},
        {"--enabled <bool=true>", "Enable or disable the override (default is true)"},
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd.examples = {
        {R"(vh vault role override 42 read-only bob --download allow --pattern ".*\.pdf$")", "Allow user 'bob' to download PDF files in the vault with ID 42, overriding the 'read-only' role."},
        {R"(vh vault role override myvault read-write developers --gid 1001 --delete deny --pattern "^/sensitive/")",
         "Deny group with GID 1001 from deleting files in the '/sensitive/' directory in the vault named 'myvault'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_role_update_override() {

}


CommandUsage VaultUsage::vault_role_override() {

}


CommandUsage VaultUsage::vault_role_list() {
    auto cmd = buildBaseUsage_();
    cmd.command = "role list";
    cmd.command_aliases = {"roles", "r l", "role l", "r list"};
    cmd.description = "List all role assignments for a specific vault.";
    cmd.positionals = {{"<vault-id|vault-name>", "ID or name of the vault"}};
    cmd.optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd.examples = {
        {"vh vault role list 42", "List all role assignments for the vault with ID 42."},
        {"vh vault role list myvault --owner alice", "List all role assignments for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_role() {
    auto cmd = buildBaseUsage_();
    cmd.command = "role";
    cmd.command_aliases = {"r"};
    cmd.description = "Manage vault role assignments and permission overrides.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (assign, remove, override, list)"}};
    cmd.examples = {
        {"vh vault role assign 42 read-only bob", "Add user 'bob' to the 'read-only' role for the vault with ID 42."},
        {"vh vault role remove myvault read-write developers --owner alice", "Remove group 'developers' from the 'read-write' role for the vault named 'myvault' owned by 'alice'."},
        {R"(vh vault role override 42 read-only bob --download allow --pattern ".*\.pdf$")", "Allow user 'bob' to download PDF files in the vault with ID 42, overriding the 'read-only' role."},
        {"vh vault role list myvault --owner alice", "List all role assignments for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

CommandUsage VaultUsage::vault_keys() {
    auto cmd = buildBaseUsage_();
    cmd.command = "keys";
    cmd.command_aliases = {"k"};
    cmd.description = "Manage API keys for accessing S3-backed vaults.";
    cmd.positionals = {{"<subcommand>", "Subcommand to execute (list, create, delete, export)"}};
    cmd.optional = {
        {"--recipient <gpg-fingerprint>", "GPG fingerprint to encrypt the exported key (for export subcommand)"},
        {"--output <file>", "Output file for the exported key (for export subcommand)"},
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd.examples = {
        {"vh vault keys list 42", "List all API keys associated with the vault with ID 42."},
        {"vh vault keys create 42 --name mykey", "Create a new API key named 'mykey' for the vault with ID 42."},
        {"vh vault keys delete 42 mykey", "Delete the API key named 'mykey' from the vault with ID 42."}
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
        {"vh vault sync 42", "Manually trigger a sync for the vault with ID 42."},
        {"vh vault sync info 42", "Show sync configuration for the vault with ID 42."},
        {"vh vault sync set 42 --sync-strategy mirror --on-sync-conflict keep_remote", "Set sync configuration for the vault with ID 42."},
        {"vh vault sync update 42 --sync-strategy cache", "Update the sync strategy for the vault with ID 42."}
    };
    return cmd;
}

CommandUsage VaultUsage::buildBaseUsage_() {
    CommandUsage cmd;
    cmd.ns = "vault";
    cmd.ns_aliases = {"v"};
    return cmd;
}

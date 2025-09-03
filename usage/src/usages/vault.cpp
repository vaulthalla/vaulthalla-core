#include "usages.hpp"
#include "CommandUsage.hpp"

using namespace vh::shell;

namespace vh::shell::vault {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    return cmd;
}

static std::shared_ptr<CommandUsage> create(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new vault. Supports local and S3-backed vaults.";
    cmd->positionals = {{"<name>", "Name of the new vault"}};
    cmd->required = {{"--local | --s3", "Type of vault to create (local or S3)"}};
    cmd->optional = {
        {"--interactive", "Run in interactive mode, prompting for missing information"},
        {"--desc <description>", "Optional description for the vault"},
        {"--quota <size|unlimited>", "Optional storage quota (e.g. 10G, 500M). Default is unlimited."},
        {"--owner <id|name>", "User ID or username of the vault owner. Default is the current user."},
    };
    cmd->groups = {
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
             {"--on-sync-conflict <keep_local | keep_remote | ask>",
              "Conflict resolution strategy during sync. Default is 'ask'."},
             {"--encrypt", "Enable upstream encryption for S3 vaults. This is the default."},
             {"--no-encrypt", "Disable upstream encryption for S3 vaults."},
             {"--accept-overwrite-waiver",
              "Acknowledge the risks of enabling encryption on an upstream s3 bucket with existing files."},
             {"--accept-decryption-waiver",
              "Acknowledge the risks of disabling encryption on an upstream s3 bucket with existing encrypted files."}
         }}
    };
    cmd->examples = {
        {"vh vault create myvault --local --desc \"My Local Vault\" --quota 10G",
         "Create a local vault with a 10GB quota."},
        {"vh vault create s3vault --s3 --api-key myapikey --bucket mybucket", "Create an S3-backed vault."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> update(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "modify", "edit"};
    cmd->description = "Update properties of an existing vault.";
    cmd->positionals = {{"<id|name>", "ID or name of the vault to update"}};
    cmd->optional = {
        {"--desc <description>", "New description for the vault"},
        {"--quota <size|unlimited>", "New storage quota (e.g. 10G, 500M). Use 'unlimited' to remove quota."},
        {"--owner <id|name>", "New owner user ID or username"},
        {"--api-key <name|id>", "New API key name or ID for S3 vaults"},
        {"--bucket <name>", "New S3 bucket name for S3 vaults"},
        {"--sync-strategy <cache|sync|mirror>", "New sync strategy for S3 vaults"},
        {"--on-sync-conflict <overwrite|keep_both|ask|keep_local|keep_remote>", "New conflict resolution strategy"},
        {"--encrypt", "Enable upstream encryption for S3 vaults. This is the default."},
        {"--no-encrypt", "Disable upstream encryption for S3 vaults."},
        {"--accept-overwrite-waiver",
         "Acknowledge the risks of enabling encryption on an upstream s3 bucket with existing files."},
        {"--accept-decryption-waiver",
         "Acknowledge the risks of disabling encryption on an upstream s3 bucket with existing encrypted files."}
    };
    cmd->examples = {
        {"vh vault update 42 --desc \"Updated Description\" --quota 20G",
         "Update the description and quota of the vault with ID 42."},
        {"vh vault update myvault --owner bob --api-key newkey --bucket newbucket --sync-strategy mirror "
         "--on-sync-conflict keep_remote --owner alice",
         "Update multiple properties of the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> remove(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"delete", "remove", "del", "rm"};
    cmd->description = "Delete an existing vault by ID or name.";
    cmd->positionals = {{"<id|name>", "ID or name of the vault to delete"}};
    cmd->optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd->examples = {
        {"vh vault delete 42", "Delete the vault with ID 42."},
        {"vh vault delete myvault --owner alice", "Delete the vault named 'myvault' owned by user 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> info(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Display detailed information about a vault.";
    cmd->positionals = {{"<id|name>", "ID or name of the vault"}};
    cmd->optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd->examples = {
        {"vh vault info 42", "Show information for the vault with ID 42."},
        {"vh vault info myvault --owner alice", "Show information for the vault named 'myvault' owned by user 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> list(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all vaults accessible to the current user.";
    cmd->optional = {
        {"--local", "Show only local vaults"},
        {"--s3", "Show only S3-backed vaults"},
        {"--limit <n>", "Limit the number of results to n vaults"},
    };
    cmd->examples = {
        {"vh vaults", "List all vaults accessible to the current user."},
        {"vh vaults --local", "List only local vaults."},
        {"vh vaults --s3 --limit 5", "List up to 5 S3-backed vaults."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> role_assign(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"assign", "add", "new", "create", "mk"};
    cmd->description = "Assign a role to a user or group for a specific vault.";
    cmd->positionals = {{"<vault-id|vault-name>", "ID or name of the vault"},
                        {"<role_id>", "ID of the role to assign"}};
    cmd->required = {{"--uid | --gid | --user | --group", "Specify the user or group to assign the role to"}};
    cmd->optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd->examples = {
        {"vh vault role assign 42 read-only bob", "Add user 'bob' to the 'read-only' role for the vault with ID 42."},
        {"vh vault role assign myvault read-write developers --owner alice",
         "Add group 'developers' to the 'read-write' role for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> role_unassign(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"unassign", "remove", "del", "rm"};
    cmd->description = "Remove a role assignment from a user or group for a specific vault.";
    cmd->positionals = {{"<vault-id|vault-name>", "ID or name of the vault"},
                        {"<role_id>", "ID of the role to unassign"}};
    cmd->required = {{"--uid | --gid | --user | --group", "Specify the user or group to remove the role from"}};
    cmd->optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd->examples = {
        {"vh vault role unassign 42 read-only bob",
         "Remove user 'bob' from the 'read-only' role for the vault with ID 42."},
        {"vh vault role unassign myvault read-write developers --owner alice",
         "Remove group 'developers' from the 'read-write' role for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> role_override_add(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"add", "new", "create", "mk"};
    cmd->description = "Add a permission override for a user or group in a specific vault role.";
    cmd->positionals = {{"<vault-id|vault-name>", "ID or name of the vault"},
                        {"<role_id>", "ID of the role to override"}};
    cmd->required = {
        {"[--user || -u || --group || -g] <id|name>", "Specify the user or group to override the permission for"},
        {"<--permission_flag>", "Permission flag to override (e.g. --download, --upload, --delete, etc.)"}
    };
    cmd->optional = {
        {"--pattern <regex>", "Optional regex pattern to scope the override to specific paths"},
        {"--enable | --disable", "Enable or disable the override (default: enabled)"},
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd->examples = {
        {R"(vh vault role override 42 read-only bob --download allow --pattern ".*\.pdf$")",
         "Allow user 'bob' to download PDF files in the vault with ID 42, overriding the 'read-only' role."},
        {R"(vh vault role override myvault read-write developers --gid 1001 --delete deny --pattern "^/sensitive/")",
         "Deny group with GID 1001 from deleting files in the '/sensitive/' directory in the vault named 'myvault'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> role_override_update(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "modify", "edit"};
    cmd->description = "Update a permission override for a user or group in a specific vault role.";
    cmd->positionals = {
        {"<vault-id|vault-name>", "ID or name of the vault"},
        {"<role_id>", "ID of the role to override"},
        {"<override_id>", "ID of the override to update"}
    };
    cmd->optional = {
        {"--pattern <regex>", "New regex pattern to scope the override to specific paths"},
        {"--enable | --disable", "Enable or disable the override (default: enabled)"},
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd->examples = {
        {R"(vh vault role override update 42 read-only 7 --deny --pattern ".*\.exe$")",
         "Update override ID 7 for user 'bob' in the vault with ID 42 to deny downloading .exe files."},
        {R"(vh vault role override update myvault read-write 3 --enabled false --owner alice)",
         "Disable override ID 3 for group 'developers' in the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> role_override_remove(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"remove", "del", "rm"};
    cmd->description = "Remove a permission override (by bit position) from a user or group in a specific vault role.";
    cmd->positionals = {
        {"<vault-id|vault-name>", "ID or name of the vault"},
        {"<role_id|role_hint>", "ID of the role or a hint (resolved within subject+vault)"},
        {"<bit_position>", "Bit position of the permission override to remove"}
    };
    cmd->required = {
        {"[--user | -u | --group | -g] <id|name>", "Specify the user or group whose role assignment owns the override"}
    };
    cmd->optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd->examples = {
        {"vh vault role override remove 42 read-only 5 -u bob",
         "Remove the bit-5 override for user 'bob' in vault 42 on role 'read-only'."},
        {"vh vault role override rm myvault read-write 3 --group developers --owner alice",
         "Remove the bit-3 override for group 'developers' in 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> role_override_list(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List permission overrides for a user or group in a specific vault role.";
    cmd->positionals = {
        {"<vault-id|vault-name>", "ID or name of the vault"},
        {"<role_id|role_hint>", "ID of the role or a hint (resolved within subject+vault)"}
    };
    cmd->required = {
        {"[--user || -u || --group || -g] <id|name>", "Specify the user or group whose role assignment to list"}
    };
    cmd->optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd->examples = {
        {"vh vault role override list 42 read-only -u bob",
         "List all overrides for user 'bob' in role 'read-only' on vault 42."},
        {"vh vault role override ls myvault read-write --group developers --owner alice",
         "List all overrides for group 'developers' in 'myvault' (owner 'alice') on role 'read-write'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> role_override(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"override", "o"};
    cmd->description = "Manage permission overrides for users or groups in a specific vault role.";
    cmd->examples = {
        {R"(vh vault role override 42 read-only -u bob --download allow --pattern ".*\.pdf$")",
         "Allow user 'bob' to download PDF files in vault 42, overriding 'read-only'."},
        {R"(vh vault role override myvault read-write --group developers --delete deny --pattern "^/sensitive/" --owner alice)",
         "Deny 'developers' from deleting files under '/sensitive/' in 'myvault' (owner 'alice')."}
    };
    cmd->subcommands = {
        role_override_add(cmd->weak_from_this()),
        role_override_update(cmd->weak_from_this()),
        role_override_remove(cmd->weak_from_this()),
        role_override_list(cmd->weak_from_this())
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> role_list(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all role assignments for a specific vault.";
    cmd->positionals = {{"<vault-id|vault-name>", "ID or name of the vault"}};
    cmd->optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"},
    };
    cmd->examples = {
        {"vh vault role list 42", "List all role assignments for the vault with ID 42."},
        {"vh vault role list myvault --owner alice",
         "List all role assignments for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> vrole(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"role", "r"};
    cmd->pluralAliasImpliesList = true;
    cmd->description = "Manage vault role assignments and permission overrides.";
    cmd->examples = {
        {"vh vault role assign 42 read-only bob", "Add user 'bob' to the 'read-only' role for the vault with ID 42."},
        {"vh vault role remove myvault read-write developers --owner alice",
         "Remove group 'developers' from the 'read-write' role for the vault named 'myvault' owned by 'alice'."},
        {R"(vh vault role override 42 read-only bob --download allow --pattern ".*\.pdf$")",
         "Allow user 'bob' to download PDF files in the vault with ID 42, overriding the 'read-only' role."},
        {"vh vault role list myvault --owner alice",
         "List all role assignments for the vault named 'myvault' owned by 'alice'."}
    };
    cmd->subcommands = {
        role_list(cmd->weak_from_this()),
        role_override(cmd->weak_from_this()),
        role_assign(cmd->weak_from_this()),
        role_unassign(cmd->weak_from_this())
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> key_list(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"list", "ls"};
    cmd->description = "List all encryption keys for all vaults (secret keys are not shown).";
    return cmd;
}

static std::shared_ptr<CommandUsage> key_create(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new encryption key for a specific vault.";
    cmd->positionals = {{"<vault-id|vault-name>", "ID or name of the vault"}};
    cmd->required = {{"--name <name>", "Name of the new encryption key"}};
    cmd->optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd->examples = {
        {"vh vault keys create 42 --name mykey", "Create a new encryption key named 'mykey' for the vault with ID 42."},
        {"vh vault keys create myvault --name backupkey --owner alice",
         "Create a new encryption key named 'backupkey' for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> key_delete(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"delete", "remove", "del", "rm"};
    cmd->description = "Delete an encryption key from a specific vault.";
    cmd->positionals = {{"<vault-id|vault-name>", "ID or name of the vault"},
                        {"<key-name>", "Name of the encryption key to delete"}};
    cmd->optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd->examples = {
        {"vh vault keys delete 42 mykey", "Delete the encryption key named 'mykey' from the vault with ID 42."},
        {"vh vault keys delete myvault backupkey --owner alice",
         "Delete the encryption key named 'backupkey' from the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> key(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"key", "k"};
    cmd->pluralAliasImpliesList = true;
    cmd->description = "Manage API keys for accessing S3-backed vaults.";
    cmd->optional = {
        {"--recipient <gpg-fingerprint>", "GPG fingerprint to encrypt the exported key (for export subcommand)"},
        {"--output <file>", "Output file for the exported key (for export subcommand)"},
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd->examples = {
        {"vh vault keys list 42", "List all API keys associated with the vault with ID 42."},
        {"vh vault keys create 42 --name mykey", "Create a new API key named 'mykey' for the vault with ID 42."},
        {"vh vault keys delete 42 mykey", "Delete the API key named 'mykey' from the vault with ID 42."}
    };
    cmd->subcommands = {
        key_list(cmd->weak_from_this()),
        key_create(cmd->weak_from_this()),
        key_delete(cmd->weak_from_this())
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> sync_info(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Display the current synchronization settings for a specific vault.";
    cmd->positionals = {{"<vault-id|vault-name>", "ID or name of the vault"}};
    cmd->optional = {
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd->examples = {
        {"vh vault sync info 42", "Show sync configuration for the vault with ID 42."},
        {"vh vault sync info myvault --owner alice",
         "Show sync configuration for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> sync_set(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"set", "update", "modify", "edit"};
    cmd->description = "Set or update synchronization settings for a specific vault.";
    cmd->positionals = {{"<vault-id|vault-name>", "ID or name of the vault"}};
    cmd->optional = {
        {"--sync-strategy <cache | sync | mirror>", "Sync strategy for S3 vaults"},
        {"--on-sync-conflict <overwrite | keep_both | ask | keep_local | keep_remote>",
         "Conflict resolution strategy during sync"},
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd->examples = {
        {"vh vault sync set 42 --sync-strategy mirror --on-sync-conflict keep_remote",
         "Set sync configuration for the vault with ID 42."},
        {"vh vault sync set myvault --sync-strategy cache --owner alice",
         "Set sync configuration for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> sync_update(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"update", "set", "modify", "edit"};
    cmd->description = "Update synchronization settings for a specific vault.";
    cmd->positionals = {{"<vault-id|vault-name>", "ID or name of the vault"}};
    cmd->optional = {
        {"--sync-strategy <cache | sync | mirror>", "New sync strategy for S3 vaults"},
        {"--on-sync-conflict <overwrite | keep_both | ask | keep_local | keep_remote>",
         "New conflict resolution strategy during sync"},
        {"--owner <id|name>", "User ID or username of the vault owner (required if using name)"}
    };
    cmd->examples = {
        {"vh vault sync update 42 --sync-strategy cache", "Update the sync strategy for the vault with ID 42."},
        {"vh vault sync update myvault --on-sync-conflict keep_local --owner alice",
         "Update the conflict resolution strategy for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> sync(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"sync", "s"};
    cmd->description = "Manage vault synchronization settings and operations.";
    cmd->positionals = {{"<vault-id|vault-name>", "ID or name of the vault"}};
    cmd->examples = {
        {"vh vault sync 42", "Manually trigger a sync for the vault with ID 42."},
        {"vh vault sync info 42", "Show sync configuration for the vault with ID 42."},
        {"vh vault sync set 42 --sync-strategy mirror --on-sync-conflict keep_remote",
         "Set sync configuration for the vault with ID 42."},
        {"vh vault sync update 42 --sync-strategy cache", "Update the sync strategy for the vault with ID 42."}
    };
    cmd->subcommands = {
        sync_info(cmd->weak_from_this()),
        sync_set(cmd->weak_from_this()),
        sync_update(cmd->weak_from_this())
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> base(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"vault", "v"};
    cmd->description = "Manage a single vault.";
    cmd->pluralAliasImpliesList = true;
    cmd->examples = {
        {"vh vault create myvault --local --desc \"My Local Vault\" --quota 10G",
         "Create a local vault with a 10GB quota."},
        {"vh vault delete myvault --owner alice", "Delete the vault named 'myvault' owned by user 'alice'."},
        {"vh vault info myvault --owner alice",
         "Show information for the vault named 'myvault' owned by user 'alice'."},
        {"vh vault update myvault --desc \"Updated Description\" --quota 20G",
         "Update the description and quota of 'myvault'."},
        {"vh vault role myvault --add bob --perm download,upload",
         "Add user 'bob' with download and upload permissions to 'myvault'."},
        {"vh vault keys myvault --list", "List all encryption keys associated with 'myvault'."},
        {"vh vault sync myvault", "Manually trigger a sync for 'myvault'."}
    };
    cmd->subcommands = {
        list(cmd->weak_from_this()),
        create(cmd->weak_from_this()),
        remove(cmd->weak_from_this()),
        info(cmd->weak_from_this()),
        update(cmd->weak_from_this()),
        vrole(cmd->weak_from_this()),
        key(cmd->weak_from_this()),
        sync(cmd->weak_from_this())
    };
    return cmd;
}

std::shared_ptr<CommandBook> get(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandBook>();
    cmd->title = "Vault Commands";
    cmd->root = base(parent);
    return cmd;
}

}
#include "usages.hpp"
#include "CommandUsage.hpp"

using namespace vh::shell;

namespace vh::shell::vault {

static std::shared_ptr<CommandUsage> buildBaseUsage(const std::weak_ptr<CommandUsage>& parent) {
    const auto cmd = std::make_shared<CommandUsage>();
    cmd->parent = parent;
    return cmd;
}

static Positional VAULT_POS = Positional::WithAliases("vault", "Name or ID of the vault", {"name", "id"});

static Optional LOCAL_CONFLICT_OPTIONAL = Optional::Single("local_conflict", "Conflict resolution strategy for local vaults",
                             "on-sync-conflict", "overwrite | keep_both | ask", "overwrite");
static Optional SYNC_STRATEGY_OPTIONAL = Optional::Multi("sync_strategy", "Sync strategy for S3 vaults", {"sync-strategy", "strategy", "ss"}, {"cache", "sync", "mirror"});
static Optional S3_CONFLICT_OPTIONAL = Optional::Multi("on_sync_conflict", "Conflict resolution strategy", {"on-sync-conflict", "conflict", "osc"}, {"overwrite", "keep_both", "ask", "keep_local", "keep_remote"});

static std::shared_ptr<CommandUsage> create(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"create", "new", "add", "mk"};
    cmd->description = "Create a new vault. Supports local and S3-backed vaults.";
    cmd->positionals = {
        Positional::Same("vault_name", "Name for the new vault")
    };
    cmd->required_flags = {
        Flag::WithAliases("type", "Type of vault to create", {"local", "s3"}),
    };
    cmd->optional_flags = {
        Flag::WithAliases("help", "Show help information about the create command", {"help", "h"}),
        Flag::WithAliases("interactive_mode", "Run in interactive mode, prompting for missing information", {"interactive", "i"})
    };
    cmd->optional = {
        Optional::ManyToOne("description", "New description for the vault", {"desc", "d"}, "description"),
        Optional::Multi("quota", "New storage quota (e.g. 10G, 500M). Use 'unlimited' to remove quota.", {"quota", "q"}, {"size", "unlimited"}),
        Optional::Multi("owner", "New owner user ID or username", {"owner", "o"}, {"id", "name"}),
    };
    cmd->groups = {
        {"Local Vault Options", {
            LOCAL_CONFLICT_OPTIONAL
         }},
        {"S3 Vault Options", {
            Optional::OneToMany("s3_api_key", "Name or ID of the API key to access the S3 bucket",
                             "api-key", {"name", "id"}),
            Optional::Single("s3_bucket", "Name of the S3 bucket", "bucket", "name"),
            SYNC_STRATEGY_OPTIONAL,
            S3_CONFLICT_OPTIONAL,
            Flag::On("enable_s3_encrypt", "Enable upstream encryption for S3 vaults. This is the default.", {"encrypt"}),
            Flag::Off("disable_s3_encrypt", "Disable upstream encryption for S3 vaults.", {"no-encrypt"}),
            Flag::Alias("accept_overwrite_waiver",
                             "Acknowledge the risks of enabling encryption on an upstream s3 bucket with existing files.",
                             "accept-overwrite-waiver"),
            Flag::Alias("accept_decryption_waiver",
                             "Acknowledge the risks of disabling encryption on an upstream s3 bucket with existing encrypted files.",
                             "accept-decryption-waiver")
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
    cmd->positionals = {
        Positional::Same("vault_name", "Name or ID of the vault to update")
    };
    cmd->optional = {
        Optional::ManyToOne("description", "New description for the vault", {"desc", "d"}, "description"),
        Optional::Multi("quota", "New storage quota (e.g. 10G, 500M). Use 'unlimited' to remove quota.", {"quota", "q"}, {"size", "unlimited"}),
        Optional::Multi("owner", "New owner user ID or username", {"owner", "o"}, {"id", "name"}),
        Optional::Multi("api_key", "New API key name or ID for S3 vault", {"api-key", "ak"}, {"name", "id"}),
        Optional::Single("bucket", "New S3 bucket name for S3 vaults", "bucket", "name"),
        SYNC_STRATEGY_OPTIONAL,
        S3_CONFLICT_OPTIONAL
    };
    cmd->optional_flags = {
        Flag::WithAliases("help", "Show help information about the update command", {"help", "h"}),
        Flag::WithAliases("interactive_mode", "Run in interactive mode, prompting for missing information", {"interactive", "i"}),
        Flag::Alias("enable_s3_encrypt", "Enable upstream encryption for S3 vaults. This is the default.", "encrypt"),
        Flag::Alias("disable_s3_encrypt", "Disable upstream encryption for S3 vaults.", "no-encrypt"),
        Flag::Alias("accept_overwrite_waiver",
                     "Acknowledge the risks of enabling encryption on an upstream s3 bucket with existing files.",
                     "accept-overwrite-waiver"),
        Flag::Alias("accept_decryption_waiver",
                     "Acknowledge the risks of disabling encryption on an upstream s3 bucket with existing encrypted files.",
                     "accept-decryption-waiver")
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
    cmd->positionals = {
        Positional::Same("vault_name", "Name or ID of the vault to delete")
    };
    cmd->optional = {
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"}),
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
    cmd->positionals = {
        Positional::Same("vault_name", "Name or ID of the vault")
    };
    cmd->optional = {
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"}),
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
    cmd->required_flags = {
        Flag::Alias("local_filter", "Show only local vaults", "local"),
        Flag::Alias("s3_filter", "Show only S3-backed vaults", "s3"),
        Flag::WithAliases("json_output", "Output the list in JSON format", {"json", "j"})
    };
    cmd->optional = {
        Optional::ManyToOne("limit", "Limit the number of vaults displayed", {"limit", "n"}, "limit")
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
    cmd->positionals = {
        Positional::Same("vault_name", "Name or ID of the vault associated with the role"),
        Positional::Alias("role", "ID of the role to assign", "id")
    };
    cmd->required = {
        Option::Multi("subject", "Specify the user or group to assign the role to", {"user", "u", "group", "g"}, {"id", "name"})
    };
    cmd->optional = {
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"}),
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
    cmd->positionals = {
        Positional::Same("vault_name", "Name or ID of the vault associated with the role"),
        Positional::Alias("role", "ID of the role to unassign", "id")
    };
    cmd->required = {
        Option::Multi("subject", "Specify the user or group to assign the role to", {"user", "u", "group", "g"}, {"id", "name"})
    };
    cmd->optional = {
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"})
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
    cmd->positionals = {
        VAULT_POS,
        Positional::Alias("role_id", "ID of the role to override", "id")
    };
    cmd->required_flags = {
        Flag::WithAliases("permissions_flags", "Permission flags to set for the new role (see 'vh permissions') min=1", ALL_SHELL_PERMS)
    };
    cmd->required = {
        Option::Multi("subject", "Specify the user or group to assign the override to", {"user", "u", "group", "g"}, {"id", "name"})
    };
    cmd->optional_flags = {
        Flag::Alias("enable", "Enable the override (default)", "enable"),
        Flag::Alias("disable", "Disable the override", "disable")
    };
    cmd->optional = {
        Optional::Single("regex_pattern", "Regex pattern to scope the override to specific paths", "pattern", "regex"),
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"})
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
        VAULT_POS,
        Positional::Alias("role_id", "ID of the role to override", "id"),
        Positional::Same("bit_position", "Bit position of the permission override to update")
    };
    cmd->optional_flags = {
        Flag::Alias("enable", "Enable the override (default)", "enable"),
        Flag::Alias("disable", "Disable the override", "disable")
    };
    cmd->optional = {
        Optional::Single("regex_pattern", "Regex pattern to scope the override to specific paths", "pattern", "regex"),
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"})
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
        VAULT_POS,
        Positional::Alias("role_id", "ID of the role to override", "id"),
        Positional::Same("bit_position", "Bit position of the permission override to update")
    };
    cmd->required = {
        Option::Multi("subject", "Specify the user or group whose override to remove", {"user", "u", "group", "g"}, {"id", "name"})
    };
    cmd->optional = {
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"})
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
        VAULT_POS,
        Positional::Alias("role_id", "ID of the role to override", "id")
    };
    cmd->required = {
        Option::Multi("subject", "Specify the user or group whose override to remove", {"user", "u", "group", "g"}, {"id", "name"})
    };
    cmd->optional = {
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"})
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
    cmd->positionals = {
        VAULT_POS,
    };
    cmd->optional = {
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"})
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

static std::shared_ptr<CommandUsage> key_export(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"export"};
    cmd->description = "Export an encryption key for a specific vault.";
    cmd->positionals = {
        Positional::WithAliases("vault", "Single vault: ID or name of the vault or 'all' for all vaults", {"id", "name", "all"}),
    };
    cmd->optional = {
        Optional::ManyToOne("gpg_recipient", "GPG fingerprint to encrypt the exported key (if blank will not encrypt)", {"recipient", "r"}, "gpg-fingerprint"),
        Optional::ManyToOne("output", "Output file for the exported key (if blank will print to stdout)", {"output", "o"}, "file"),
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"})
    };
    cmd->examples = {
        {"vh vault keys export 42 --output keyfile.pem --recipient ABCDEF1234567890",
         "Export the encryption key for the vault with ID 42 to 'keyfile.pem', encrypted for the GPG recipient with fingerprint 'ABCDEF1234567890'."},
        {"vh vault keys export myvault --owner alice --output myvault_key.pem",
         "Export the encryption key for the vault named 'myvault' owned by 'alice' to 'myvault_key.pem' (unencrypted)."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> key_rotate(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"rotate", "renew"};
    cmd->description = "Rotate the encryption key for a specific vault.";
    cmd->positionals = {
        Positional::WithAliases("vault", "Single vault: ID or name of the vault or 'all' for all vaults", {"id", "name", "all"}),
    };
    cmd->optional = {
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"})
    };
    cmd->optional_flags = {
        Flag::WithAliases("sync_now", "Immediately synchronize the vault after key rotation", {"now", "sync-now", "sync"})
    };
    cmd->examples = {
        {"vh vault keys rotate 42 --sync-now", "Rotate the encryption key for the vault with ID 42 and immediately sync."},
        {"vh vault keys rotate myvault --owner alice",
         "Rotate the encryption key for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> key(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"key", "k"};
    cmd->pluralAliasImpliesList = true;
    cmd->description = "Manage encryption keys for vaults.";
    cmd->examples = {
        {"vh vault keys export 42 --output keyfile.pem --recipient ABCDEF1234567890",
         "Export the encryption key for the vault with ID 42 to 'keyfile.pem', encrypted for the GPG recipient with fingerprint 'ABCDEF1234567890'."},
        {"vh vault keys rotate myvault --owner alice",
         "Rotate the encryption key for the vault named 'myvault' owned by 'alice'."}
    };
    cmd->subcommands = {
        key_export(cmd->weak_from_this()),
        key_rotate(cmd->weak_from_this())
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> sync_info(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"info", "show", "get"};
    cmd->description = "Display the current synchronization settings for a specific vault.";
    cmd->positionals = {
        VAULT_POS
    };
    cmd->optional = {
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"})
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
    cmd->positionals = {
        VAULT_POS
    };
    cmd->optional = {
        Optional::Multi("owner", "User ID or username of the vault owner (required if using name)", {"owner", "o"}, {"id", "name"})
    };
    cmd->groups = {
        {"Local Vault Options", {
            LOCAL_CONFLICT_OPTIONAL
         }},
        {"S3 Vault Options", {
            SYNC_STRATEGY_OPTIONAL,
            S3_CONFLICT_OPTIONAL
         }}
    };
    cmd->examples = {
        {"vh vault sync set 42 --sync-strategy mirror --on-sync-conflict keep_remote",
         "Set sync configuration for the vault with ID 42."},
        {"vh vault sync set myvault --sync-strategy cache --owner alice",
         "Set sync configuration for the vault named 'myvault' owned by 'alice'."}
    };
    return cmd;
}

static std::shared_ptr<CommandUsage> sync(const std::weak_ptr<CommandUsage>& parent) {
    auto cmd = buildBaseUsage(parent);
    cmd->aliases = {"sync", "s"};
    cmd->description = "Manage vault synchronization settings and operations.";
    cmd->positionals = {
        VAULT_POS
    };
    cmd->examples = {
        {"vh vault sync 42", "Manually trigger a sync for the vault with ID 42."},
        {"vh vault sync info 42", "Show sync configuration for the vault with ID 42."},
        {"vh vault sync set 42 --sync-strategy mirror --on-sync-conflict keep_remote",
         "Set sync configuration for the vault with ID 42."},
        {"vh vault sync update 42 --sync-strategy cache", "Update the sync strategy for the vault with ID 42."}
    };
    cmd->subcommands = {
        sync_info(cmd->weak_from_this()),
        sync_set(cmd->weak_from_this())
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
        {"vh vault update 42 --desc \"Updated Description\" --quota 20G",
         "Update the description and quota of the vault with ID 42."},
        {"vh vault delete myvault --owner alice", "Delete the vault named 'myvault' owned by user 'alice'."},
        {"vh vault info 42", "Show information for the vault with ID 42."},
        {"vh vaults", "List all vaults accessible to the current user."},
        {"vh vault role assign 42 read-only bob", "Add user 'bob' to the 'read-only' role for the vault with ID 42."},
        {"vh vault keys export 42 --output keyfile.pem --recipient ABCDEF1234567890",
         "Export the encryption key for the vault with ID 42 to 'keyfile.pem', encrypted for the GPG recipient with fingerprint 'ABCDEF1234567890'."},
        {"vh vault sync 42", "Manually trigger a sync for the vault with ID 42."}
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
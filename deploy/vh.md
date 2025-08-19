% vh(1) Vaulthalla CLI
% Cooper Larson
% August 2025

# NAME

vh, vaulthalla \- command-line interface for the Vaulthalla server daemon

# SYNOPSIS

**vh** [*global-options*] *command* [*args*]

# DESCRIPTION

The `vh` tool (also accessible via the `vaulthalla` symlink) lets you interact with a running Vaulthalla server daemon. It provides administrative control over users, groups, roles, storage vaults, synchronization tasks, and API keys. Most commands accept either human-readable names or numeric IDs; where names could be ambiguous, you can disambiguate with an additional flag (e.g., `--owner` for vaults).

Use `vh <command> --help` to see command-specific help.

# GLOBAL OPTIONS

- `-h`, `--help`  
  Show top-level help.

- `-v`, `--version`  
  Show version information.

# COMMAND LIST

| NAME      | ALIASES            | DESCRIPTION                    |
|-----------|--------------------|--------------------------------|
| `api-key` | `--ak`, `--api`    | Manage a single API key        |
| `api-keys`| —                  | List API keys                  |
| `group`   | `-g`               | Manage a single group          |
| `groups`  | `--lg`             | List groups                    |
| `help`    | `?`, `-h`, `--help`| Show help info                 |
| `roles`   | —                  | Manage roles                   |
| `user`    | `-u`               | Manage a single user           |
| `users`   | —                  | List users                     |
| `vault`   | —                  | Manage a single vault          |
| `vaults`  | `--ls`             | List vaults                    |
| `version` | `-v`, `--version`  | Show version information       |
| `sync`    | —                  | Control background sync tasks  |

# SUBCOMMANDS

## users / user

    user [create | new] --name <name> --role <role> [--email <email>] [--linux-uid <uid>]
    user [delete | rm] <name>
    user [info | get] <name>
    user [update | set] <name> [--name <name>] [--email <email>] [--role <role>] [--linux-uid <uid>]

- `user create` creates a new user with the given name, role, and optional email/Linux UID.
- `user delete` removes a user by name.
- `user info` shows details.
- `user update` changes name/email/role/Linux UID.

## groups / group

    groups [list]
    group create <name> [--desc <description>] [--linux-gid <id>]
    group delete <name> [--owner-id <id>]
    group info <name | id>
    group update <name | id> [--name <new_name>] [--desc <description>] [--linux-gid <id>]
    group add-user <group_name | gid> <user_name | uid>
    group remove-user <group_name | gid> <user_name | uid>
    group list-users <group_name | gid> [--owner-id <id>]

Manage POSIX-style groups and membership. Use names or IDs for both groups and users.

## vaults / vault

    === Vault Creation ===
    vault create <name> --local
        [--on-sync-conflict <overwrite | keep_both | ask>]
        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]
        [--owner <id | name>]

    vault create <name> --s3
        --api-key <name | id> --bucket <name>
        [--sync-strategy <cache | sync | mirror>]
        [--on-sync-conflict <keep_local | keep_remote | ask>]
        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]
        [--owner <id | name>]

    === Vault Update ===
    vault (update | set) <id>
        [--api-key <name | id>] [--bucket <name>]
        [--sync-strategy <cache | sync | mirror>]
        [--on-sync-conflict <overwrite | keep_both | ask | keep_local | keep_remote>]
        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]
        [--owner <id | name>]

    vault (update | set) <name> <owner>
        [--api-key <name | id>] [--bucket <name>]
        [--sync-strategy <cache | sync | mirror>]
        [--on-sync-conflict <overwrite | keep_both | ask | keep_local | keep_remote>]
        [--desc <text>] [--quota <size(T | G | M | B)> | unlimited]
        [--owner <id | name>]

    === Common Commands ===
    vault delete <id>
    vault delete <name> --owner <id | name>
    vault info   <id>
    vault info   <name> [--owner <id | name>]
    vault assign <id> <role_id> --[uid | gid | user | group] <id | name>
    vault assign <name> <role_id> --[uid | gid | user | group] <id | name> --owner <id | name>

    === Encryption Key Management ===
    vault keys export  <vault_id | name | all>  [--recipient <fingerprint>] [--output <file>] [--owner <id | name>]
    vault keys rotate  <vault_id | name | all>  [--owner <id | name>]
    vault keys inspect <vault_id | name>        [--owner <id | name>]

- `--sync-strategy`:
    - `cache`: keep a local cache fronting remote storage.
    - `sync`: two-way file synchronization.
    - `mirror`: one-way mirror of remote to local.
- `--on-sync-conflict`:
    - Local: `overwrite`, `keep_both`, `ask`
    - Remote/S3 context: `keep_local`, `keep_remote`, `ask`
- `--quota` accepts sizes like `500G`, `2T`, `100M`. Use `unlimited` to remove quotas.
- `vault assign` attaches a **vault role** (see *roles*) to a subject:
    - Subject selectors: `--uid` / `--user` for users; `--gid` / `--group` for groups.
- `--owner` is required when a vault name is ambiguous or duplicated across owners.

## roles

    roles list [--user | --vault] [--json] [--limit <n>]
    roles info <id>
    roles info <name> [--user | --vault]
    roles create <name> --type <user | vault> [--from <id | name>] [<permission_flags>]
    roles update <id> [--name <new_name>] [<permission_flag>]
    roles delete <id>

Create and manage **role templates**. There are two kinds:
- **User roles** (global/admin scopes)
- **Vault roles** (per-vault access scopes)

### User-role permission flags

You can use the shorthand `--manage-*` to set a flag, or `--set-*` / `--unset-*` explicitly.

    --manage-encryption-keys   | --set-manage-encryption-keys   | --unset-manage-encryption-keys
    --manage-admins            | --set-manage-admins            | --unset-manage-admins
    --manage-users             | --set-manage-users             | --unset-manage-users
    --manage-groups            | --set-manage-groups            | --unset-manage-groups
    --manage-vaults            | --set-manage-vaults            | --unset-manage-vaults
    --manage-roles             | --set-manage-roles             | --unset-manage-roles
    --manage-api-keys          | --set-manage-api-keys          | --unset-manage-api-keys
    --audit-log-access         | --set-audit-log-access         | --unset-audit-log-access
    --create-vaults            | --set-create-vaults            | --unset-create-vaults

> **Danger:** `--manage-encryption-keys` grants export/rotation authority. Assign with extreme caution.

### Vault-role permission flags

For per-vault access control:

    --migrate-data
    --manage-access
    --manage-tags
    --manage-metadata
    --manage-versions
    --manage-file-locks
    --share
    --sync
    --create
    --download
    --delete
    --rename
    --move
    --list

Each can be toggled with `--set-<flag>` / `--unset-<flag>` or enabled via shorthand (e.g., `--share`).

## api-keys / api-key

    api-keys [create | new] --name <name> --access <accessKey> --secret <secret> --region <region=auto> --endpoint <endpoint> --provider <provider>
    api-keys [create | new] -n <name> -a <accessKey> -s <secret> -r <region=auto> -e <endpoint> -p <provider>
        provider options: [aws | cloudflare-r2 | wasabi | backblaze-b2 | digitalocean | minio | ceph | storj | other]
    api-keys [delete | rm] <id>
    api-keys [info | get] <id>
    api-keys list [--json]

API keys can be referenced by name or ID when configuring S3-backed vaults.

## sync

    vh sync status
    vh sync trigger

Display sync status or request a sync run.

## help / version

- `vh help` or `vh <command> --help` shows usage.
- `vh version` (or `-v`, `--version`) prints version details.

# EXAMPLES

Create a user and put them in a group:

    vh user create --name "alice" --role "operator" --email "alice@example.com" --linux-uid 1001
    vh group create engineering --desc "Core eng"
    vh group add-user engineering alice

Create an S3-backed vault and grant Alice read/list/share:

    vh api-keys create --name r2-main --provider cloudflare-r2 --access ABC --secret DEF --endpoint https://<accid>.r2.cloudflarestorage.com --region auto
    vh vault create team-archive --s3 --api-key r2-main --bucket team-archive \
      --sync-strategy cache --on-sync-conflict ask --desc "Team archive"

    # Assume a vault role 'reader' has flags: --list --download --share
    vh roles info reader --vault
    vh vault assign team-archive  <role_id_of_reader> --user alice --owner <owner_id_or_name>

Rotate keys for all vaults (admin-only):

    vh vault keys rotate all

List roles as JSON (for scripting):

    vh roles list --vault --json --limit 50

# BEHAVIOR & CONVENTIONS

- **IDs vs names:** Most commands accept either; `<id>` is numeric; `<name>` is case-sensitive unless documented otherwise. When a name is ambiguous, supply `--owner` (for vaults) or use the numeric ID directly.
- **JSON output:** Listing commands (e.g., `roles list`, `api-keys list`) accept `--json` to produce machine-readable output.
- **Quotas:** Sizes accept suffixes `T`, `G`, `M`, `B`. `unlimited` removes enforcement.

# SECURITY NOTES

- **Encryption key export:** `vault keys export` should be restricted to super-admins. If a non-super-admin holds `manage-encryption-keys`, audit logs will warn accordingly.
- **Least privilege:** Prefer narrowly-scoped vault roles over broad user-role grants.
- **Sync conflicts:** Choose `ask` in high-risk environments to avoid silent overwrites.

# FILES

- `/etc/vaulthalla/config.yaml` — Main configuration file. (If managed by the web UI, comments may be preserved/reinserted when writing back.)

# EXIT STATUS

`vh` exits with status `0` on success and a non-zero status on failure.

# SEE ALSO

`vaulthalla` daemon, system logs, project documentation.

# AUTHOR

Cooper Larson and the Vaulthalla contributors.

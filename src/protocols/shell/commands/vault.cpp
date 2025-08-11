#include "protocols/shell/commands/vault.hpp"
#include "protocols/shell/Parser.hpp"
#include "protocols/shell/Router.hpp"

#include "services/ServiceDepsRegistry.hpp"
#include "storage/StorageManager.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "types/Vault.hpp"
#include "types/S3Vault.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace vh::shell;
using namespace vh::types;
using namespace vh::storage;
using namespace vh::database;

// ---------- tiny helpers ----------
static std::optional<std::string_view> optVal(const CommandCall& c, const std::string_view key) {
    for (const auto& [k, v] : c.options) if (k == key) return v.value_or(std::string_view{});
    return std::nullopt;
}
static bool hasFlag(const CommandCall& c, const std::string_view key) {
    for (const auto& [k, v] : c.options) if (k == key) return !v.has_value();
    return false;
}
static bool hasKey(const CommandCall& c, const std::string_view key) {
    return std::ranges::any_of(c.options, [&key](const auto& kv) { return kv.key == key; });
}
static std::optional<int> parseInt(const std::string_view sv) {
    if (sv.empty()) return std::nullopt;
    int v = 0; bool neg = false; size_t i = 0;
    if (sv[0] == '-') { neg = true; i = 1; }
    for (; i < sv.size(); ++i) {
        char c = sv[i];
        if (c < '0' || c > '9') return std::nullopt;
        v = v*10 + (c - '0');
    }
    return neg ? -v : v;
}

// ---------- pretty/usage ----------
static CommandResult usage_vault_root() {
    return {
        0,
        "Usage:\n"
        "  vault create <name> [--local|--s3] [--encrypt] [--desc <text>]\n"
        "  vault delete <name> [--force]\n"
        "  vault info   <name>  [--json]\n",
        ""
    };
}
static CommandResult usage_vaults_list() {
    return {
        0,
        "Usage:\n"
        "  vaults [--local|--s3] [--limit <n>] [--json]\n",
        ""
    };
}

// ---------- validators ----------
static CommandResult invalid(std::string msg) { return {2, "", std::move(msg)}; }
static CommandResult ok(std::string out) { return {0, std::move(out), ""}; }

// ---------- handlers ----------
static CommandResult handle_vault_create(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault create: missing <name>");
    if (call.positionals.size() > 1) return invalid("vault create: too many arguments");

    const std::string_view name = call.positionals[0];

    const bool f_local = hasFlag(call, "local");
    const bool f_s3    = hasFlag(call, "s3");
    if (f_local && f_s3) return invalid("vault create: --local and --s3 are mutually exclusive");

    const bool f_encrypt = hasFlag(call, "encrypt");
    const auto descOpt   = optVal(call, "desc"); // optional string; empty string allowed

    // TODO: validate name (length, charset)
    // TODO: normalize backend default: if neither --local nor --s3, infer from config
    // TODO: check uniqueness: does vault <name> already exist?
    // TODO: create in DB (and filesystem/S3) transactionally
    // TODO: if f_encrypt, generate key material, store metadata (respect your key-export policy)

    // Pretend success
    std::string out = "Created vault '" + std::string(name) + "'";
    if (f_local) out += " (local)";
    if (f_s3)    out += " (s3)";
    if (f_encrypt) out += " [encrypted]";
    if (descOpt) out += " - desc set";
    out += "\n";
    return ok(out);
}

static CommandResult handle_vault_delete(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault delete: missing <name>");
    if (call.positionals.size() > 1) return invalid("vault delete: too many arguments");

    const std::string_view name = call.positionals[0];
    const bool f_force = hasFlag(call, "force");

    // TODO: lookup vault by name; 404 if not found
    // TODO: if not --force, prompt/require confirm channel (if your CLI supports it) or bail
    // TODO: ensure vault empty or mark for async purge (depending on policy)
    // TODO: delete metadata, detach mounts, revoke tokens, purge S3 prefixes if applicable
    // TODO: audit log the deletion (who, when, where)

    return ok("Deleted vault '" + std::string(name) + (f_force ? "' (forced)\n" : "'\n"));
}

static CommandResult handle_vault_info(const CommandCall& call) {
    if (call.positionals.empty()) return invalid("vault info: missing <name>");
    if (call.positionals.size() > 1) return invalid("vault info: too many arguments");

    const std::string_view name = call.positionals[0];
    const bool json = hasFlag(call, "json");

    // TODO: fetch metadata: backend (local/s3), size, object count, created_at, encrypted, owner, ACLs
    // TODO: include mount status, sync status, last sync, checksum policy, retention windows

    if (json) {
        // TODO: serialize stable JSON schema
        return ok(
            "{ \"name\":\"" + std::string(name) + "\", \"backend\":\"local\", \"encrypted\":true }\n"
        );
    } else {
        // pretty text
        std::string out;
        out += "Name:      " + std::string(name) + "\n";
        out += "Backend:   local\n";
        out += "Encrypted: yes\n";
        out += "Created:   2025-08-10T12:34:56Z\n";
        out += "Owner:     you\n";
        out += "Size:      42 GiB (69,420 objects)\n";
        // TODO: real values
        return ok(out);
    }
}

static CommandResult handle_vaults_list(const CommandCall& call) {
    // Validate options
    const bool f_local = hasFlag(call, "local");
    const bool f_s3    = hasFlag(call, "s3");
    if (f_local && f_s3) return invalid("vaults: --local and --s3 are mutually exclusive");
    if (!call.positionals.empty()) return invalid("vaults: unexpected positional arguments");

    int limit = 100;
    if (auto lim = optVal(call, "limit")) {
        if (lim->empty()) return invalid("vaults: --limit requires a value");
        auto parsed = parseInt(*lim);
        if (!parsed || *parsed <= 0) return invalid("vaults: --limit must be a positive integer");
        limit = *parsed;
    }
    const bool json = hasFlag(call, "json");

    // TODO: query DB with filters: backend == local/s3 (if specified), ORDER BY created_at DESC LIMIT <limit>
    // TODO: pagination (next token) if needed for large sets
    // TODO: respect authZ: only list vaults the caller can see

    if (json) {
        auto out = nlohmann::json(VaultQueries::listVaults()).dump(4);
        out.push_back('\n');
        return ok(out);
    }

    return ok(to_string(VaultQueries::listVaults()));
}

// ---------- registration ----------
void vh::shell::registerVaultCommands(const std::shared_ptr<Router>& r) {
    // Parent "vault" with subcommands
    r->registerCommand("vault", "Manage a single vault",
        [](const CommandCall& call) -> CommandResult {
            // Expect: create/delete/info as first positional
            if (call.positionals.empty())
                return usage_vault_root();

            const std::string_view sub = call.positionals[0];
            CommandCall subcall = call;
            // shift subcommand off positionals
            subcall.positionals.erase(subcall.positionals.begin());

            if (sub == "create" || sub == "c") {
                // Allowed flags: --local, --s3, --encrypt, --desc <text>
                // TODO: reject unknown flags for this subcommand (validate here if you want)
                return handle_vault_create(subcall);
            }
            if (sub == "delete" || sub == "rm") {
                // Allowed flags: --force
                return handle_vault_delete(subcall);
            }
            if (sub == "info" || sub == "get" || sub == "describe") {
                // Allowed flags: --json
                return handle_vault_info(subcall);
            }

            return invalid("vault: unknown subcommand '" + std::string(sub) + "'. Use: create | delete | info");
        },
        /*aliases*/{"v"});

    // Top-level "vaults" (list)
    r->registerCommand("vaults", "List vaults",
        [](const CommandCall& call) -> CommandResult {
            // Fast help
            if (hasKey(call, "help") || hasKey(call, "h")) return usage_vaults_list();
            // Allowed flags: --local | --s3 (mutually exclusive), --limit <n>, --json
            return handle_vaults_list(call);
        },
        /*aliases*/{"ls"});
}

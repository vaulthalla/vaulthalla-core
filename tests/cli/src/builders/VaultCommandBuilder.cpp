#include "CommandBuilder.hpp"
#include "CommandUsage.hpp"
#include "generators.hpp"
#include "types/Vault.hpp"
#include "database/Queries/UserQueries.hpp"

using namespace vh::test::cli;
using namespace vh::shell;
using namespace vh::types;
using namespace vh::database;
using vh::types::Vault;

VaultCommandBuilder::VaultCommandBuilder(const std::shared_ptr<shell::UsageManager>& usage, const std::shared_ptr<CLITestContext>& ctx)
    : CommandBuilder(usage, ctx, "vault"), vaultAliases_(ctx) {}

std::string VaultCommandBuilder::updateAndResolveVar(const std::shared_ptr<Vault>& entity, const std::string& field) {
    const std::string usagePath = "vault/update";

    if (vaultAliases_.isName(field)) {
        entity->name = generateName(usagePath);
        return entity->name;
    }

    if (vaultAliases_.isDescription(field)) {
        if (coin()) {
            entity->description = "This is a description for vault " + entity->name;
            return entity->description;
        }
        entity->description = std::string(""); // clear description
        return entity->description;
    }

    if (vaultAliases_.isQuota(field)) {
        entity->setQuotaFromStr(generateQuotaStr(usagePath));
        return entity->quotaStr();
    }

    throw std::runtime_error("EntityFactory: unsupported vault field for update: " + field);
}

std::string VaultCommandBuilder::chooseVaultType() {
    // Bias to local so tests don’t demand S3 specifics unless you want them to
    return coin() ? "local" : "local"; // flip second to "s3" when you’re ready
}

std::string VaultCommandBuilder::vaultRef(const std::shared_ptr<Vault>& v, bool& usedName) {
    // Prefer ID for stability; sometimes exercise the name path (requires owner)
    if (v->id > 0 && !coin()) { usedName = false; return std::to_string(v->id); }
    usedName = true; return v->name;
}

void VaultCommandBuilder::emitOwnerIfName(std::ostringstream& oss, const std::shared_ptr<Vault>& v, bool usedName) {
    if (!usedName) return;
    // your usage allows --owner id|name; we’ll pass an id if we have it
    if (v->owner_id > 0) oss << " --owner "   << v->owner_id;
    else if (const auto owner = UserQueries::getUserById(v->owner_id)) oss << " --owner name " << owner->name;
    else oss << " --owner id 1"; // last-resort: test user 1
}

// ---------------- core ----------------

std::string VaultCommandBuilder::create(const std::shared_ptr<Vault>& v) {
    const auto cmd = root_->findSubcommand("create");
    if (!cmd) throw std::runtime_error("vault.create usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases) << ' ';

    // positional: vault name or id (prefer name on create)
    bool usedName = true;
    oss << v->name << ' ';

    // required flags: type
    const auto type = chooseVaultType(); // "local" or "s3"
    oss << "--" << type;

    // optional
    if (!v->description.empty() && coin()) oss << " --" << randomAlias(std::vector<std::string>{"desc","d"}) << ' ' << quoted(v->description);
    if (v->quota > 0 && coin())           oss << " --" << randomAlias(std::vector<std::string>{"quota","q"}) << ' ' << v->quotaStr();

    // owner (create by name requires owner if server needs it; harmless otherwise)
    emitOwnerIfName(oss, v, usedName);

    // s3 group (only if you start returning "s3" in chooseVaultType)
    // if (type == "s3") {
    //     if (coin()) oss << " --api-key " << (v->s3_api_key.empty() ? "default-key" : v->s3_api_key);
    //     if (coin()) oss << " --bucket "  << (v->s3_bucket.empty()  ? "default-bucket" : v->s3_bucket);
    //     if (coin()) oss << " --sync-strategy " << randomAlias(std::vector<std::string>{"cache","sync","mirror"});
    //     if (coin()) oss << " --on-sync-conflict " << randomAlias(std::vector<std::string>{"keep_local","keep_remote","ask"});
    //     // encryption toggles
    //     if (coin()) oss << " --encrypt";
    //     else if (coin()) oss << " --no-encrypt";
    //     if (coin()) oss << " --accept-overwrite-waiver";
    // } else {
    //     // local group
    //     if (coin()) oss << " --on-sync-conflict overwrite";
    // }

    // sometimes run interactive
    // if (coin(10000, 1500)) oss << " --interactive";

    return oss.str();
}

std::string VaultCommandBuilder::update(const std::shared_ptr<Vault>& v) {
    const auto cmd = root_->findSubcommand("update");
    if (!cmd) throw std::runtime_error("vault.update usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases) << ' ';
    bool usedName = false;
    oss << vaultRef(v, usedName);
    emitOwnerIfName(oss, v, usedName);

    if (!v->description.empty() && coin()) oss << " --" << randomAlias(std::vector<std::string>{"desc","d"}) << ' ' << quoted(v->description);
    if (v->quota > 0 && coin())           oss << " --" << randomAlias(std::vector<std::string>{"quota","q"}) << ' ' << v->quotaStr();

    // s3-ish knobs (harmless on local if server ignores)
    // if (coin()) oss << " --sync-strategy " << randomAlias(std::vector<std::string>{"cache","sync","mirror"});
    // if (coin()) oss << " --on-sync-conflict " << randomAlias(std::vector<std::string>{"keep_local","keep_remote","ask"});
    // if (coin()) oss << (coin() ? " --encrypt" : " --no-encrypt");
    // if (coin()) oss << (coin() ? " --accept-overwrite-waiver" : " --accept-decryption-waiver");

    // if (coin(10000, 1200)) oss << " --interactive";

    return oss.str();
}

std::string VaultCommandBuilder::remove(const std::shared_ptr<Vault>& v) {
    const auto cmd = root_->findSubcommand("delete");
    if (!cmd) throw std::runtime_error("vault.delete usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases) << ' ';
    if (coin()) oss << v->id;
    else {
        oss << v->name;
        emitOwnerIfName(oss, v, /*usedName=*/true);
    }
    return oss.str();
}

std::string VaultCommandBuilder::info(const std::shared_ptr<Vault>& v) {
    const auto cmd = root_->findSubcommand("info");
    if (!cmd) throw std::runtime_error("vault.info usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases) << ' ';
    bool usedName = false;
    oss << vaultRef(v, usedName);
    emitOwnerIfName(oss, v, usedName);
    return oss.str();
}

std::string VaultCommandBuilder::list() {
    const auto cmd = root_->findSubcommand("list");
    if (!cmd) throw std::runtime_error("vault.list usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    if (coin()) oss << " --local";
    if (coin()) oss << " --s3";
    if (coin()) oss << " --limit " << (5 + (generateRandomIndex(1000) % 10));
    if (coin(10000, 2000)) oss << " --json";
    return oss.str();
}

// ---------------- extras ----------------

std::string VaultCommandBuilder::sync_set(const std::shared_ptr<Vault>& v) {
    const auto sync = root_->findSubcommand("sync");     if (!sync) throw std::runtime_error("vault.sync not found");
    const auto set  = sync->findSubcommand("set");       if (!set)  throw std::runtime_error("vault.sync.set not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(sync->aliases) << ' ' << randomAlias(set->aliases) << ' ';
    bool usedName = false;
    oss << vaultRef(v, usedName);
    emitOwnerIfName(oss, v, usedName);

    oss << " --sync-strategy " << randomAlias(std::vector<std::string>{"cache","sync","mirror"});
    oss << " --on-sync-conflict " << randomAlias(std::vector<std::string>{"keep_local","keep_remote","ask"});
    return oss.str();
}

std::string VaultCommandBuilder::sync_info(const std::shared_ptr<Vault>& v) {
    const auto sync = root_->findSubcommand("sync");     if (!sync) throw std::runtime_error("vault.sync not found");
    const auto info = sync->findSubcommand("info");      if (!info) throw std::runtime_error("vault.sync.info not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(sync->aliases) << ' ' << randomAlias(info->aliases) << ' ';
    bool usedName = false;
    oss << vaultRef(v, usedName);
    emitOwnerIfName(oss, v, usedName);
    return oss.str();
}

std::string VaultCommandBuilder::sync_trigger(const std::shared_ptr<Vault>& v) {
    // the parent "sync" is also executable per your examples
    const auto sync = root_->findSubcommand("sync");     if (!sync) throw std::runtime_error("vault.sync not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(sync->aliases) << ' ';
    bool usedName = false;
    oss << vaultRef(v, usedName);
    emitOwnerIfName(oss, v, usedName);
    return oss.str();
}

std::string VaultCommandBuilder::key_export(const std::shared_ptr<Vault>& v) {
    const auto key = root_->findSubcommand("key");       if (!key)  throw std::runtime_error("vault.key not found");
    const auto exp = key->findSubcommand("export");      if (!exp)  throw std::runtime_error("vault.key.export not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(key->aliases) << ' ' << randomAlias(exp->aliases) << ' ';
    // your usage allows "all" as positional; we’ll target a single vault
    bool usedName = false;
    oss << vaultRef(v, usedName);
    emitOwnerIfName(oss, v, usedName);

    if (coin()) oss << " --output " << (v->name.empty() ? "vault_key.pem" : v->name + "_key.pem");
    if (coin()) oss << " --recipient " << "ABCDEF1234567890";
    return oss.str();
}

std::string VaultCommandBuilder::key_rotate(const std::shared_ptr<Vault>& v) {
    const auto key = root_->findSubcommand("key");       if (!key)  throw std::runtime_error("vault.key not found");
    const auto rot = key->findSubcommand("rotate");      if (!rot)  throw std::runtime_error("vault.key.rotate not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(key->aliases) << ' ' << randomAlias(rot->aliases) << ' ';
    bool usedName = false;
    oss << vaultRef(v, usedName);
    emitOwnerIfName(oss, v, usedName);
    if (coin()) oss << " --sync-now";
    return oss.str();
}

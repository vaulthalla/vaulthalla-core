#include "CommandBuilder.hpp"
#include "CommandUsage.hpp"
#include "generators.hpp"

using namespace vh::test::cli;
using namespace vh::types;
using namespace vh::shell;

VaultRoleCommandBuilder::VaultRoleCommandBuilder(const std::shared_ptr<TestUsageManager>& usage)
    : CommandBuilder(usage, "role") {}

static std::optional<std::string> resolveVar(const std::string& name, const std::shared_ptr<VaultRole>& role) {
    if (name == "id" || name == "role_id") return std::to_string(role->id);
    if (name == "name" || name == "role_name") return role->name;
    if (name == "description" || name == "desc") return role->description;
    if (name == "permissions" || name == "perms") return std::to_string(role->permissions);
    throw std::runtime_error("VaultRoleCommandBuilder: unsupported vault role field for resolveVar: " + name);
}

static std::string randomizePrimaryPositional(const std::shared_ptr<VaultRole>& entity) {
    if (generateRandomIndex(10000) < 5000) return std::to_string(entity->id);
    return entity->name + " --vault";
}

std::string VaultRoleCommandBuilder::create(const std::shared_ptr<VaultRole>& entity) {
    const auto cmd = root_->findSubcommand("create");
    if (!cmd) throw std::runtime_error("VaultRoleCommandBuilder: 'create' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases) << " vault ";

    for (const auto& opt : cmd->required) {
        if (opt.label == "type") continue; // skip type, always "vault" for vault roles
        oss << ' ' << randomAlias(opt.option_tokens);
        if (auto val = resolveVar(opt.option_tokens[0], entity); val) oss << ' ' << *val;
        else throw std::runtime_error("VaultRoleCommandBuilder: unsupported vault role field for create: " + opt.option_tokens[0]);
    }

    for (const auto& option : cmd->optional) {
        if (rand() % 2 == 0) {
            oss << ' ' << randomAlias(option.option_tokens);
            if (auto val = resolveVar(option.option_tokens[0], entity); val) oss << ' ' << *val;
            else throw std::runtime_error("VaultRoleCommandBuilder: unsupported vault role field for create: " + option.option_tokens[0]);
        }
    }

    const auto permsFlags = randomVaultPermsFlags();
    if (permsFlags.empty()) throw std::runtime_error("VaultRoleCommandBuilder: failed to generate random permissions flags for vault role creation");
    for (const auto& pf : permsFlags) oss << ' ' << pf;

    return oss.str();
}

std::string VaultRoleCommandBuilder::update(const std::shared_ptr<VaultRole>& entity) {
    const auto cmd = root_->findSubcommand("update");
    if (!cmd) throw std::runtime_error("VaultRoleCommandBuilder: 'update' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases) << " vault ";
    oss << randomizePrimaryPositional(entity);

    if (generateRandomIndex(20000) < 11000) {
        entity->name = generateRoleName(EntityType::VAULT_ROLE, "role/update");
        oss << " --name " << entity->name;
    }

    for (const auto& flag : randomVaultPermsFlags()) oss << ' ' << flag;

    return oss.str();
}

std::string VaultRoleCommandBuilder::info(const std::shared_ptr<VaultRole>& entity) {
    const auto cmd = root_->findSubcommand("info");
    if (!cmd) throw std::runtime_error("VaultRoleCommandBuilder: 'info' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases) << " vault ";
    oss << randomizePrimaryPositional(entity);

    return oss.str();
}

std::string VaultRoleCommandBuilder::list() {
    const auto cmd = root_->findSubcommand("list");
    if (!cmd) throw std::runtime_error("VaultRoleCommandBuilder: 'list' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    if (generateRandomIndex(10000) < 5000) oss << " --vault";
    for (const auto& flag : cmd->optional_flags)
        if (generateRandomIndex(10000) < 5000) oss << " --" << randomAlias(flag.aliases);

    return oss.str();
}

std::string VaultRoleCommandBuilder::remove(const std::shared_ptr<VaultRole>& entity) {
    const auto cmd = root_->findSubcommand("delete");
    if (!cmd) throw std::runtime_error("VaultRoleCommandBuilder: 'delete' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    if (generateRandomIndex(10000) < 5000) oss << ' ' << entity->id;
    else oss << ' ' << entity->name << " --vault";

    return oss.str();
}

#include "CommandBuilder.hpp"
#include "CommandUsage.hpp"
#include "generators.hpp"

using namespace vh::test::cli;
using namespace vh::types;
using namespace vh::shell;

UserRoleCommandBuilder::UserRoleCommandBuilder(const std::shared_ptr<shell::UsageManager>& usage, const std::shared_ptr<CLITestContext>& ctx)
    : CommandBuilder(usage, ctx, "role"), userRoleAliases_(ctx) {}

std::string UserRoleCommandBuilder::updateAndResolveVar(const std::shared_ptr<UserRole>& entity, const std::string& field) {
    const std::string usagePath = "role/update";

    if (userRoleAliases_.isName(field)) {
        entity->name = generateRoleName(EntityType::USER_ROLE, usagePath);
        return entity->name;
    }

    if (userRoleAliases_.isDescription(field)) {
        entity->description = "Updated user role description";
        return entity->description;
    }

    if (userRoleAliases_.isPermissions(field)) {
        entity->permissions = generateBitmask(ADMIN_SHELL_PERMS.size());
        return entity->permissions_to_flags_string();
    }

    throw std::runtime_error("EntityFactory: unsupported user role field for update: " + field);
}

static std::optional<std::string> resolveVar(const std::string& name, const std::shared_ptr<UserRole>& role) {
    if (name == "id" || name == "role_id") return std::to_string(role->id);
    if (name == "name" || name == "role_name") return role->name;
    if (name == "description" || name == "desc") return role->description;
    if (name == "permissions" || name == "perms") return role->permissions_to_flags_string();
    if (name == "type" || name == "role_type") return role->type;
    throw std::runtime_error("UserRoleCommandBuilder: unsupported user role field for resolveVar: " + name);
}

static std::string randomizePrimaryPositional(const std::shared_ptr<UserRole>& entity) {
    if (generateRandomIndex(10000) < 5000) return std::to_string(entity->id);
    return entity->name + " --user";
}

std::string UserRoleCommandBuilder::create(const std::shared_ptr<UserRole>& entity) {
    const auto cmd = root_->findSubcommand("create");
    if (!cmd) throw std::runtime_error("UserRoleCommandBuilder: 'create' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases) << " user " << entity->name;

    for (const auto& opt : cmd->required) {
        if (opt.label == "type") continue; // skip type, always "user" for user roles
        oss << ' ' << randomFlagAlias(opt.option_tokens);
        if (auto val = resolveVar(opt.option_tokens[0], entity); val) oss << ' ' << *val;
        else throw std::runtime_error("UserRoleCommandBuilder: unsupported user role field for create: " + opt.option_tokens[0]);
    }

    for (const auto& option : cmd->optional) {
        if (coin()) {
            if (option.option_tokens[0] == "from") {
                if (!ctx_->userRoles.empty() && coin())
                    oss << ' ' << randomFlagAlias(option.option_tokens) << ' ' << std::to_string(ctx_->randomUserRole()->id);
                continue;
            }

            oss << ' ' << randomFlagAlias(option.option_tokens);
            if (auto val = resolveVar(option.option_tokens[0], entity); val) oss << ' ' << *val;
            else throw std::runtime_error("UserRoleCommandBuilder: unsupported user role field for create: " + option.option_tokens[0]);
        }
    }

    const auto permsFlags = randomUserPermsFlags();
    if (permsFlags.empty()) throw std::runtime_error("UserRoleCommandBuilder: failed to generate random permissions flags for user role creation");
    for (const auto& pf : permsFlags) oss << ' ' << pf;

    return oss.str();
}

std::string UserRoleCommandBuilder::update(const std::shared_ptr<types::UserRole>& entity) {
    const auto cmd = root_->findSubcommand("update");
    if (!cmd) throw std::runtime_error("UserRoleCommandBuilder: 'update' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases) << " user ";
    oss << randomizePrimaryPositional(entity);

    if (coin()) oss << " --name " << updateAndResolveVar(entity, "name");
    if (coin()) oss << " --desc " << quoted(updateAndResolveVar(entity, "description"));
    if (coin()) oss << ' ' << updateAndResolveVar(entity, "permissions");

    return oss.str();
}

std::string UserRoleCommandBuilder::info(const std::shared_ptr<UserRole>& entity) {
    const auto cmd = root_->findSubcommand("info");
    if (!cmd) throw std::runtime_error("UserRoleCommandBuilder: 'info' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases) << " user ";
    oss << randomizePrimaryPositional(entity);

    return oss.str();
}

std::string UserRoleCommandBuilder::list() {
    const auto cmd = root_->findSubcommand("list");
    if (!cmd) throw std::runtime_error("UserRoleCommandBuilder: 'list' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    if (generateRandomIndex(10000) < 5000) oss << " --user";
    for (const auto& flag : cmd->optional_flags)
        if (generateRandomIndex(10000) < 5000) oss << " --" << randomAlias(flag.aliases);

    return oss.str();
}

std::string UserRoleCommandBuilder::remove(const std::shared_ptr<UserRole>& entity) {
    const auto cmd = root_->findSubcommand("delete");
    if (!cmd) throw std::runtime_error("UserRoleCommandBuilder: 'delete' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    if (generateRandomIndex(10000) < 5000) oss << ' ' << entity->id;
    else oss << ' ' << entity->name << " --user";

    return oss.str();
}


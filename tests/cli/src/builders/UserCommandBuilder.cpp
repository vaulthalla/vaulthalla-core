#include "CommandBuilder.hpp"
#include "TestUsageManager.hpp"
#include "CommandUsage.hpp"
#include "generators.hpp"

using namespace vh::test::cli;
using namespace vh::types;
using namespace vh::shell;

UserCommandBuilder::UserCommandBuilder(const std::shared_ptr<TestUsageManager>& usage)
    : CommandBuilder(usage, "user") {}

static std::optional<std::string> resolveVar(const std::string& name, const std::shared_ptr<User>& user) {
    if (name == "id" || name == "user_id") return std::to_string(user->id);
    if (name == "name" || name == "username") return user->name;
    if (name == "email") return user->email;
    if (name == "role" || name == "role_id") return std::to_string(user->role->id);
    throw std::runtime_error("UserCommandBuilder: unsupported user field for resolveVar: " + name);
}

std::string UserCommandBuilder::create(const std::shared_ptr<User>& entity) {
    const auto cmd = root_->findSubcommand("create");
    if (!cmd) throw std::runtime_error("UserCommandBuilder: 'create' command usage not found");
    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    for (const auto& opt : cmd->required) {
        oss << ' ' << randomAlias(opt.option_tokens);
        if (auto val = resolveVar(opt.option_tokens[0], entity); val) oss << ' ' << *val;
        else throw std::runtime_error("UserCommandBuilder: unsupported user field for create: " + opt.option_tokens[0]);
    }
    return oss.str();
}

std::string UserCommandBuilder::update(const std::shared_ptr<User>& entity) {
    const auto cmd = root_->findSubcommand("update");
    if (!cmd) throw std::runtime_error("UserCommandBuilder: 'update' command usage not found");
    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    oss << ' ' << (generateRandomIndex(10000) < 5000 ? std::to_string(entity->id) : entity->name);
    const auto numFieldsToUpdate = std::max(static_cast<size_t>(1), generateRandomIndex(cmd->optional.size()));
    std::unordered_set<std::string> updated;
    while (updated.size() < numFieldsToUpdate) {
        const auto opt = cmd->optional[generateRandomIndex(cmd->optional.size())];
        for (const auto& t : opt.option_tokens) {
            if (!updated.contains(t)) {
                oss << ' ' << t;
                if (auto val = resolveVar(t, entity); val) oss << ' ' << *val;
                else throw std::runtime_error("UserCommandBuilder: unsupported user field for update: " + t);
                updated.insert(t);
                break;
            }
        }
    }
    return oss.str();
}

std::string UserCommandBuilder::remove(const std::shared_ptr<User>& entity) {
    const auto cmd = root_->findSubcommand("delete");
    if (!cmd) throw std::runtime_error("UserCommandBuilder: 'delete' command usage not found");
    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    oss << ' ' << (generateRandomIndex(10000) < 5000 ? std::to_string(entity->id) : entity->name);
    return oss.str();
}

std::string UserCommandBuilder::info(const std::shared_ptr<User>& entity) {
    const auto cmd = root_->findSubcommand("info");
    if (!cmd) throw std::runtime_error("UserCommandBuilder: 'info' command usage not found");
    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    oss << ' ' << (generateRandomIndex(10000) < 5000 ? std::to_string(entity->id) : entity->name);
    return oss.str();
}

std::string UserCommandBuilder::list() {
    const auto cmd = root_->findSubcommand("list");
    if (!cmd) throw std::runtime_error("UserCommandBuilder: 'list' command usage not found");
    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    return oss.str();
}

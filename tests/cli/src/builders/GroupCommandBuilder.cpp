#include "CommandBuilder.hpp"
#include "CommandUsage.hpp"
#include "generators.hpp"

using namespace vh::test::cli;
using namespace vh::types;
using namespace vh::shell;

GroupCommandBuilder::GroupCommandBuilder(const std::shared_ptr<TestUsageManager>& usage)
    : CommandBuilder(usage, "group") {}

static std::optional<std::string> resolveVar(const std::string& name, const std::shared_ptr<Group>& group) {
    if (name == "id" || name == "group_id") return std::to_string(group->id);
    if (name == "name" || name == "group_name") return group->name;
    if (name == "description" || name == "desc") return group->description;
    throw std::runtime_error("GroupCommandBuilder: unsupported group field for resolveVar: " + name);
}

static std::string randomizePrimaryPositional(const std::shared_ptr<Group>& entity) {
    if (generateRandomIndex(10000) < 5000) return std::to_string(entity->id);
    return entity->name;
}

std::string GroupCommandBuilder::create(const std::shared_ptr<Group>& entity) {
    const auto cmd = root_->findSubcommand("create");
    if (!cmd) throw std::runtime_error("GroupCommandBuilder: 'create' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    for (const auto& opt : cmd->required) {
        oss << ' ' << randomAlias(opt.option_tokens);
        if (auto val = resolveVar(opt.option_tokens[0], entity); val) oss << ' ' << *val;
        else throw std::runtime_error("GroupCommandBuilder: unsupported group field for create: " + opt.option_tokens[0]);
    }
    return oss.str();
}

std::string GroupCommandBuilder::update(const std::shared_ptr<Group>& entity) {
    const auto cmd = root_->findSubcommand("update");
    if (!cmd) throw std::runtime_error("GroupCommandBuilder: 'update' command usage not found");

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
                else throw std::runtime_error("GroupCommandBuilder: unsupported group field for update: " + t);
                updated.insert(t);
                break;
            }
        }
    }
    return oss.str();
}

std::string GroupCommandBuilder::remove(const std::shared_ptr<Group>& entity) {
    const auto cmd = root_->findSubcommand("delete");
    if (!cmd) throw std::runtime_error("GroupCommandBuilder: 'delete' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    oss << ' ' << randomizePrimaryPositional(entity);
    return oss.str();
}

std::string GroupCommandBuilder::info(const std::shared_ptr<Group>& entity) {
    const auto cmd = root_->findSubcommand("info");
    if (!cmd) throw std::runtime_error("GroupCommandBuilder: 'info' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    if (generateRandomIndex(10000) < 5000) oss << ' ' << entity->id;
    else oss << ' ' << entity->name;
    return oss.str();
}

std::string GroupCommandBuilder::list() {
    const auto cmd = root_->findSubcommand("list");
    if (!cmd) throw std::runtime_error("GroupCommandBuilder: 'list' command usage not found");

    std::ostringstream oss;
    oss << "vh " << randomAlias(root_->aliases) << ' ' << randomAlias(cmd->aliases);
    for (const auto& flag : cmd->optional_flags)
        if (generateRandomIndex(10000) < 5000) oss << " --" << randomAlias(flag.aliases);
    return oss.str();
}

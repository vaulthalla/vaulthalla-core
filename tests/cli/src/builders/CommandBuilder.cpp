#include "CommandBuilder.hpp"
#include "TestUsageManager.hpp"
#include "CommandUsage.hpp"
#include "generators.hpp"

using namespace vh::test::cli;

CommandBuilder<>::CommandBuilder(const std::shared_ptr<TestUsageManager>& usage, const std::string& rootTopLevelAlias) {
    if (!usage) throw std::runtime_error("CommandBuilder: usage manager is null");
    const auto cmd = usage->resolve(rootTopLevelAlias);
    if (!cmd) throw std::runtime_error("CommandBuilder: command usage not found for root: " + rootTopLevelAlias);
    root_ = cmd;
}

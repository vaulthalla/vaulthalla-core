#include "TestUsageManager.hpp"

using namespace vh::test;

std::shared_ptr<CommandUsage> TestUsageManager::getFilteredTestUsage() const {
    const std::vector<std::vector<std::string>> exclude = {
        {"api-key", "delete"},
        {"role", "delete"},
        {"user", "delete"},
        {"group", "delete"},
        {"vault", "delete"},
        {"secrets", "set"},
        {"secrets", "delete"},
        {"vault", "keys", "export"}
    };

    auto copy = std::make_shared<CommandUsage>(*root_);

    for (const auto& path : exclude) {
        if (path.empty()) continue;

        std::shared_ptr<CommandUsage> current = copy;
        std::shared_ptr<CommandUsage> parent;
        std::string target;

        // Walk to parent of the target
        for (std::size_t i = 0; i < path.size(); ++i) {
            const auto& alias = path[i];
            auto it = std::ranges::find_if(
                current->subcommands.begin(),
                current->subcommands.end(),
                [&](const std::shared_ptr<CommandUsage>& c) {
                    return std::ranges::any_of(c->aliases, [&](const std::string& a) { return a == alias; });
                });

            if (it == current->subcommands.end()) {
                break; // Path doesn't exist, skip
            }

            if (i == path.size() - 1) {
                // Last element — delete this
                current->subcommands.erase(it);
            } else {
                parent = current;
                current = *it;
            }
        }
    }

    return copy;
}

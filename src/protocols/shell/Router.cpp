#include "protocols/shell/Router.hpp"
#include <fmt/core.h>
#include <cctype>

using namespace vh::shell;

void Router::registerCommand(std::string name, std::string desc, CommandHandler handler,
                                      const std::initializer_list<std::string> aliases)
{
    std::string key = normalize(std::move(name));
    CommandInfo info{std::move(desc), std::move(handler), {}};

    for (auto& alias : aliases) {
        info.aliases.insert(normalize(alias));
        aliasMap_[normalize(alias)] = key;
    }

    commands_[key] = std::move(info);
}

void Router::execute(const std::string& name, const std::vector<std::string>& args) const {
    auto normalized = normalize(name);
    if (aliasMap_.contains(normalized)) normalized = aliasMap_.at(normalized);

    if (!commands_.contains(normalized))
        throw std::invalid_argument(fmt::format("Unknown command or alias: {}", name));

    return commands_.at(normalized).handler(args);
}

void Router::listCommands() const {
    for (auto& [name, info] : commands_) {
        fmt::print("{} (aliases: {}) - {}\n",
            name,
            joinAliases(info.aliases),
            info.description);
    }
}

std::string Router::normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (const char c : s) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    return out;
}

std::string Router::joinAliases(const std::unordered_set<std::string>& aliases) {
    if (aliases.empty()) return "-";
    std::string out;
    for (auto it = aliases.begin(); it != aliases.end(); ++it) {
        if (it != aliases.begin()) out += ", ";
        out += *it;
    }
    return out;
}

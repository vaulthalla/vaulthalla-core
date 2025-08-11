#include "protocols/shell/Router.hpp"
#include "protocols/shell/Token.hpp"
#include "protocols/shell/Parser.hpp"
#include "logging/LogRegistry.hpp"

#include <fmt/core.h>
#include <cctype>
#include <stdexcept>

using namespace vh::shell;
using namespace vh::logging;

void Router::registerCommand(const std::string& name, std::string desc, CommandHandler handler,
                             const std::initializer_list<std::string> aliases)
{
    std::string key = normalize(name);

    CommandInfo info{std::move(desc), std::move(handler), {}};

    // record aliases
    for (const auto& alias : aliases) {
        std::string a = normalize_alias(alias);

        // Optional: avoid alias collisions
        if (auto it = aliasMap_.find(a); it != aliasMap_.end() && it->second != key) {
            LogRegistry::shell()->warn("Alias '{}' already mapped to '{}'; skipping duplicate for '{}'",
                                       a, it->second, key);
            continue;
        }
        info.aliases.insert(a);
        aliasMap_[a] = key;
    }

    commands_[key] = std::move(info);
}

std::string Router::canonicalFor(const std::string& nameOrAlias) const {
    auto n = normalize(nameOrAlias);
    if (commands_.contains(n)) return n;

    const auto a = normalize_alias(nameOrAlias);
    if (aliasMap_.contains(a)) return aliasMap_.at(a);

    return n;
}

void Router::execute(const CommandCall& call) const {
    LogRegistry::shell()->info("[execute(call)] Executing command...");
    call.print();

    const auto canonical = canonicalFor(call.name);
    if (!commands_.contains(canonical))
        throw std::invalid_argument(fmt::format("Unknown command or alias: {}", call.name));
    return commands_.at(canonical).handler(call);
}

void Router::execute(const std::string& name, const std::vector<std::string>& args) const {
    // Legacy shim: wrap into a CommandCall with only positionals.
    CommandCall call;
    call.name = name;
    call.positionals = args;

    LogRegistry::shell()->info("[execute(name, args)] Executing command...");
    call.print();

    // call.options remains empty
    execute(call);
}

void Router::executeLine(const std::string& line) const {
    const auto tokens = tokenize(line);
    const auto call = parseTokens(tokens);

    LogRegistry::shell()->info("[executeLine] Executing command...");
    call.print();

    if (call.name.empty()) {
        // No-op on empty line; alternatively, throw if you prefer strict mode
        return;
    }
    execute(call);
}

void Router::listCommands() const {
    for (const auto& [name, info] : commands_) fmt::print("{}\n", name);
}

std::string Router::normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

// In Router.hpp / .cpp
std::string Router::strip_leading_dashes(const std::string& s) {
    size_t i = 0; while (i < s.size() && s[i] == '-') ++i;
    return s.substr(i);
}

std::string Router::normalize_alias(const std::string& s) {
    return normalize(strip_leading_dashes(s)); // <- key change
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

void CommandInfo::print(const std::string& canonical) const {
    auto logger = LogRegistry::shell();
    logger->info("{} (aliases: {}) - {}", canonical, Router::joinAliases(aliases), description);
}

#include "protocols/shell/Router.hpp"
#include "protocols/shell/Token.hpp"
#include "protocols/shell/Parser.hpp"
#include "services/LogRegistry.hpp"
#include "util/cmdLineHelpers.hpp"
#include "types/User.hpp"
#include "protocols/shell/CommandUsage.hpp"
#include "protocols/shell/usage/ShellUsage.hpp"

#include <fmt/core.h>
#include <cctype>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <sys/ioctl.h>

using namespace vh::shell;
using namespace vh::logging;


void Router::registerCommand(const CommandUsage& usage, CommandHandler handler) {
    std::string key = normalize(usage.command.empty() ? usage.ns : usage.ns + " " + usage.command);

    CommandInfo info{usage.description.empty() ? "No description provided." : usage.description, std::move(handler), {}};

    for (const std::string& alias : usage.ns_aliases) {
        std::string a = normalize_alias(alias);
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
    std::string n = normalize(nameOrAlias);
    if (commands_.contains(n)) return n;

    std::string a = normalize_alias(nameOrAlias);
    if (aliasMap_.contains(a)) return aliasMap_.at(a);

    return n; // unknown; let caller error
}

CommandResult Router::execute(const CommandCall& call) const {
    const auto canonical = canonicalFor(call.name);
    LogRegistry::shell()->debug("[Router] Executing command: '{}'", canonical);
    if (!commands_.contains(canonical))
        return {2, ShellUsage::all().filterTopLevelOnly().toText(), fmt::format("Unknown command or alias: {}", call.name)};
    return commands_.at(canonical).handler(call);
}

CommandResult Router::executeLine(const std::string& line, const std::shared_ptr<types::User>& user) const {
    LogRegistry::shell()->debug("[Router] Executing line: '{}'", line);
    auto call   = parseTokens(tokenize(line));
    call.user = user;

    if (call.name.empty()) return CommandResult{1, "", "no command provided"};
    return execute(call);
}

std::string Router::normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (const unsigned char c : s) out.push_back(static_cast<char>(std::tolower(c)));
    return out;
}

std::string Router::strip_leading_dashes(const std::string& s) {
    size_t i = 0; while (i < s.size() && s[i] == '-') ++i;
    return std::string{s.substr(i)};
}

std::string Router::normalize_alias(const std::string& s) {
    return normalize(strip_leading_dashes(s));
}

std::string Router::joinAliases(const std::unordered_set<std::string>& aliases) {
    if (aliases.empty()) return "-";
    std::vector v(aliases.begin(), aliases.end());
    std::ranges::sort(v.begin(), v.end());
    std::string out;
    for (size_t i=0;i<v.size();++i) { if (i) out += ", "; out += v[i]; }
    return out;
}

std::string Router::pretty_alias(const std::string& a) {
    if (a == "?") return "?";
    if (a.size() == 1) return fmt::format("-{}", a);
    return fmt::format("--{}", a);
}

#include "protocols/shell/Router.hpp"
#include "protocols/shell/Token.hpp"
#include "protocols/shell/Parser.hpp"
#include "services/LogRegistry.hpp"
#include "util/cmdLineHelpers.hpp"
#include "types/User.hpp"

#include <fmt/core.h>
#include <cctype>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <sys/ioctl.h>

using namespace vh::shell;
using namespace vh::logging;

void Router::registerCommand(const std::string& name,
                             const std::string& desc,
                             CommandHandler handler,
                             const std::initializer_list<std::string> aliases)
{
    std::string key = normalize(name);

    CommandInfo info{std::string(desc), std::move(handler), {}};

    for (const std::string& alias : aliases) {
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
    const auto canonical = canonicalFor(call.name); // call.name is string_view
    LogRegistry::shell()->debug("[Router] Executing command: '{}'", canonical);
    if (!commands_.contains(canonical))
        throw std::invalid_argument(fmt::format("Unknown command or alias: {}", call.name));
    return commands_.at(canonical).handler(call);
}

CommandResult Router::executeLine(const std::string& line, const std::shared_ptr<types::User>& user) const {
    LogRegistry::shell()->debug("[Router] Executing line: '{}'", line);
    auto call   = parseTokens(tokenize(line));
    call.user = user;

    if (call.name.empty()) return CommandResult{1, "", "no command provided"};
    return execute(call);
}

std::string Router::listCommands() const {
    struct Row { std::string name, aliases, desc; };

    std::vector<std::string> keys;
    keys.reserve(commands_.size());
    for (const auto& [key, _] : commands_) keys.push_back(key);
    std::ranges::sort(keys.begin(), keys.end());

    auto join_aliases = [](const std::unordered_set<std::string>& s) {
        if (s.empty()) return std::string("-");
        std::vector v(s.begin(), s.end());
        std::ranges::sort(v.begin(), v.end());
        std::string out;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) out += ", ";
            out += pretty_alias(v[i]);
        }
        return out;
    };

    std::vector<Row> rows;
    rows.reserve(keys.size());
    size_t name_w = 4, alias_w = 7;

    for (const auto& k : keys) {
        const auto& info = commands_.at(k);
        Row r{k, join_aliases(info.aliases), info.description};
        name_w  = std::max(name_w,  r.name.size());
        alias_w = std::max(alias_w, r.aliases.size());
        rows.push_back(std::move(r));
    }

    std::string out;
    out.reserve(64 + rows.size() * 64);
    out += "Vaulthalla commands:\n";
    out += fmt::format("  {:{}}  {:{}}  {}\n", "NAME", name_w, "ALIASES", alias_w, "DESCRIPTION");
    out += fmt::format("  {:-<{}}  {:-<{}}  {}\n", "", name_w, "", alias_w, "-----------");

    const int W = term_width();
    const size_t left = 2 + name_w + 2 + alias_w + 2;
    const size_t desc_width = (W > static_cast<int>(left + 10)) ? (W - left) : 40;

    for (const auto& r : rows) {
        auto wrapped = wrap_text(r.desc, desc_width);
        out += fmt::format("  {:{}}  {:{}}  {}\n",
                           r.name, name_w, r.aliases, alias_w,
                           wrapped.substr(0, wrapped.find('\n')));

        size_t pos = wrapped.find('\n');
        while (pos != std::string::npos) {
            const size_t next = wrapped.find('\n', pos + 1);
            auto line = wrapped.substr(pos + 1, next - (pos + 1));
            out += fmt::format("  {:{}}  {:{}}  {}\n", "", name_w, "", alias_w, line);
            pos = next;
        }
    }

    return out + "\n" + fmt::format("Use 'vh <command> --help' for more information on any command.\n"
                         "Use 'vh <alias> --help' for more information on any alias.\n");
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

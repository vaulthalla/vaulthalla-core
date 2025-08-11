#include "protocols/shell/Router.hpp"
#include "protocols/shell/Token.hpp"
#include "protocols/shell/Parser.hpp"
#include "logging/LogRegistry.hpp"

#include <fmt/core.h>
#include <cctype>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

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

CommandResult Router::execute(const CommandCall& call) const {
    const auto canonical = canonicalFor(call.name);
    if (!commands_.contains(canonical))
        throw std::invalid_argument(fmt::format("Unknown command or alias: {}", call.name));
    return commands_.at(canonical).handler(call);
}

CommandResult Router::executeLine(const std::string& line) const {
    const auto tokens = tokenize(line);
    const auto call = parseTokens(tokens);

    if (call.name.empty()) return CommandResult(1, "", "no command provided");
    return execute(call);
}

std::string Router::listCommands() const {
    struct Row { std::string name, aliases, desc; };

    // 1) Collect rows in deterministic order
    std::vector<std::string> keys;
    keys.reserve(commands_.size());
    for (const auto& [key, _] : commands_) keys.push_back(key);
    std::ranges::sort(keys.begin(), keys.end());

    std::vector<Row> rows;
    rows.reserve(keys.size());

    auto join_aliases = [](const std::unordered_set<std::string>& s) {
        if (s.empty()) return std::string("-");
        std::vector<std::string> v(s.begin(), s.end());
        std::ranges::sort(v.begin(), v.end()); // stable-ish
        std::string out;
        for (size_t i = 0; i < v.size(); ++i) {
            if (i) out += ", ";
            out += pretty_alias(v[i]);
        }
        return out;
    };

    size_t name_w = std::max<size_t>(4, 0);    // width for "NAME"
    size_t alias_w = std::max<size_t>(7, 0);   // width for "ALIASES"

    for (const auto& k : keys) {
        const auto& info = commands_.at(k);
        Row r{k, join_aliases(info.aliases), info.description};
        name_w  = std::max(name_w,  r.name.size());
        alias_w = std::max(alias_w, r.aliases.size());
        rows.push_back(std::move(r));
    }

    // 2) Render
    std::string out;
    out.reserve(64 + rows.size() * 64);
    out += "vaulthalla commands:\n";
    out += fmt::format("  {:{}}  {:{}}  {}\n", "NAME", name_w, "ALIASES", alias_w, "DESCRIPTION");
    out += fmt::format("  {:-<{}}  {:-<{}}  {}\n", "", name_w, "", alias_w, "-----------");

    const int W = term_width();
    const size_t left = 2 + name_w + 2 + alias_w + 2; // "  " + name + "  " + aliases + "  "
    const size_t desc_width = (W > left + 10) ? (W - left) : 40;

    for (const auto& r : rows) {
        auto wrapped = wrap_text(r.desc, desc_width);
        // first line
        out += fmt::format("  {:{}}  {:{}}  {}\n", r.name, name_w, r.aliases, alias_w,
                           wrapped.substr(0, wrapped.find('\n')));
        // continuation lines
        size_t pos = wrapped.find('\n');
        while (pos != std::string::npos) {
            const size_t next = wrapped.find('\n', pos + 1);
            auto line = wrapped.substr(pos + 1, next - (pos + 1));
            out += fmt::format("  {:{}}  {:{}}  {}\n", "", name_w, "", alias_w, line);
            pos = next;
        }
    }

    return out;
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

std::string Router::pretty_alias(const std::string& a) {
    if (a == "?") return "?";
    if (a.size() == 1) return fmt::format("-{}", a);
    return fmt::format("--{}", a);
}

int Router::term_width() {
    if (!isatty(STDOUT_FILENO)) return 80;
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    const char* c = std::getenv("COLUMNS");
    if (c) { int n = std::atoi(c); if (n > 0) return n; }
    return 80;
}

std::string Router::wrap_text(std::string_view s, size_t width) {
    std::string out; size_t col = 0;
    size_t i = 0;
    while (i < s.size()) {
        // take a word
        size_t j = i;
        while (j < s.size() && s[j] != ' ') ++j;
        size_t wlen = j - i;
        if (col && col + 1 + wlen > width) { out += '\n'; col = 0; }
        if (col) { out += ' '; ++col; }
        out.append(s.substr(i, wlen));
        col += wlen;
        while (j < s.size() && s[j] == ' ') ++j;
        i = j;
    }
    return out;
}

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <unordered_map>

namespace vh::shell {

struct Entry {
    std::string label;
    std::string desc;
    std::vector<std::string> aliases;
};

struct GroupedOptions {
    std::string title;
    std::vector<Entry> items;
};

struct Example {
    std::string cmd;
    std::string note;
};

struct ColorTheme {
    bool enabled = true;

    std::string header = "\033[1;36m"; // section titles (bold cyan)
    std::string command = "\033[1;32m"; // command name (bold green)
    std::string key = "\033[33m";      // left column keys (yellow)
    std::string dim = "\033[2m";       // dim/secondary text
    std::string reset = "\033[0m";

    [[nodiscard]] std::string maybe(const std::string& code) const { return enabled ? code : ""; }
    [[nodiscard]] std::string H() const { return maybe(header); }
    [[nodiscard]] std::string C() const { return maybe(command); }
    [[nodiscard]] std::string K() const { return maybe(key); }
    [[nodiscard]] std::string D() const { return maybe(dim); }
    [[nodiscard]] std::string R() const { return maybe(reset); }
};

class CommandUsage {
public:
    std::string ns, command;
    std::vector<std::string> ns_aliases, command_aliases;
    std::string description;
    std::optional<std::string> synopsis;  // if empty, synthesized
    std::vector<Entry> positionals, required, optional;
    std::vector<GroupedOptions> groups;
    std::vector<Example> examples;

    int term_width = 100;  // target width for str()
    std::size_t max_key_col = 30; // cap left column width
    bool show_aliases = true;
    ColorTheme theme{};

    [[nodiscard]] std::string str() const;
    [[nodiscard]] std::string basicStr(bool splitHeader = false) const;
    [[nodiscard]] std::string markdown() const;

private:
    // internal helpers (exposed for testing if you want)
    [[nodiscard]] std::string buildSynopsis_() const;
    [[nodiscard]] static std::string normalizePositional_(const std::string& s);
    [[nodiscard]] static std::string bracketizeIfNeeded_(const std::string& s, bool square);
    [[nodiscard]] static std::string joinAliasesInline_(const std::string& primary,
                                                        const std::vector<std::string>& aliases,
                                                        const std::string& sep);
    [[nodiscard]] static std::string joinAliasesCode_(const std::string& primary,
                                                      const std::vector<std::string>& aliases);
};

class CommandBook {
public:
    std::string title;
    std::vector<CommandUsage> commands;
    std::optional<ColorTheme> book_theme;

    [[nodiscard]] std::string str() const;
    [[nodiscard]] std::string basicStr() const;
    [[nodiscard]] std::string markdown() const;

    void operator<<(const CommandBook& other);

    [[nodiscard]] std::unordered_map<std::string, CommandBook> splitByNamespace() const;
    [[nodiscard]] CommandBook filterByNamespace(const std::string& ns) const;
    [[nodiscard]] CommandBook filterTopLevelOnly() const;
};

} // namespace vh::shell

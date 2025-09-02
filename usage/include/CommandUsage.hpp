#pragma once

#include "ColorTheme.hpp"

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <memory>

namespace vh::shell {

struct Entry {
    std::string label;
    std::string desc;
    std::vector<std::string> aliases;

    // Constructors to support brace-initialization with or without aliases
    Entry(std::string l, std::string d)
        : label(std::move(l)), desc(std::move(d)), aliases{} {}

    Entry(std::string l, std::string d, std::vector<std::string> a)
        : label(std::move(l)), desc(std::move(d)), aliases(std::move(a)) {}

    Entry() = default;
};

struct GroupedOptions {
    std::string title;
    std::vector<Entry> items;

    GroupedOptions(std::string t, std::vector<Entry> i)
        : title(std::move(t)), items(std::move(i)) {}

    GroupedOptions() = default;
};

struct Example {
    std::string cmd;
    std::string note;
};

class CommandUsage : public std::enable_shared_from_this<CommandUsage> {
public:
    std::vector<std::string> aliases;
    std::string description;
    std::optional<std::string> synopsis;  // if empty, synthesized
    std::weak_ptr<CommandUsage> parent;
    std::vector<std::shared_ptr<CommandUsage>> subcommands;
    std::vector<Entry> positionals, required, optional;
    std::vector<GroupedOptions> groups;
    std::vector<Example> examples;
    bool pluralAliasImpliesList = false;

    int term_width = 100;  // target width for str()
    std::size_t max_key_col = 30; // cap left column width
    bool show_aliases = true;
    ColorTheme theme{};

    [[nodiscard]] std::string primary() const;

    [[nodiscard]] std::string str() const;
    [[nodiscard]] std::string basicStr() const;
    [[nodiscard]] std::string markdown() const;

    [[nodiscard]] bool matches(const std::string& alias) const;

private:
    // internal helpers (exposed for testing if you want)
    [[nodiscard]] std::string buildSynopsis_() const;
    [[nodiscard]] static std::string normalizePositional_(const std::string& s);
    [[nodiscard]] static std::string bracketizeIfNeeded_(const std::string& s, bool square);
    [[nodiscard]] std::string constructCmdString() const;
    [[nodiscard]] std::string joinSubcommandsInline(const std::string& sep = " | ") const;
    [[nodiscard]] std::string joinAliasesInline_(const std::string& sep = " | ") const;
    [[nodiscard]] std::string joinAliasesCode_();
    void emitCommand(std::ostringstream& out,
        const std::shared_ptr<CommandUsage>& command = nullptr,
        bool spaceLines = false) const;
};

} // namespace vh::shell

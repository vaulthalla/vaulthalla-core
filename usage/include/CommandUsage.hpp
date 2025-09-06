#pragma once

#include "ColorTheme.hpp"

#include <string>
#include <vector>
#include <optional>
#include <cstddef>
#include <memory>
#include <unordered_map>

namespace vh::shell {

struct Entry {
    std::string label;
    std::string desc;

    // Constructors to support brace-initialization with or without aliases
    Entry(std::string l, std::string d)
        : label(std::move(l)), desc(std::move(d)) {}

    Entry() = default;
};

struct Positional : Entry {
    std::vector<std::string> aliases;

    Positional(std::string l, std::string d) : Entry(std::move(l), std::move(d)) {}

    Positional(std::string l, std::string d, std::vector<std::string> a)
        : Entry(std::move(l), std::move(d)), aliases(std::move(a)) {}

    Positional() = default;
};

struct Flag : Positional {
    bool default_state = false;

    Flag(std::string l, std::string d) : Positional(std::move(l), std::move(d)) {}

    Flag(std::string l, std::string d, std::string a)
        : Positional(std::move(l), std::move(d), std::vector{std::move(a)}) {}

    Flag(std::string l, std::string d, bool s) : Positional(std::move(l), std::move(d)), default_state(s) {}

    Flag(std::string l, std::string d, bool s, std::vector<std::string> a)
        : Positional(std::move(l), std::move(d), std::move(a)), default_state(s) {}

    Flag() = default;
};

struct Option : Entry {
    // allows for better grouping of flag aliases and possible values
    // e.g. { "--user", { { "<name>", "<id>" } } } for "--user <name|id>"
    //   or { "--group", { { "<group>", "<id>" } } } for "-g | --group"
    std::unordered_map<std::string, std::vector<std::string>> aliases{};

    Option(std::string l, std::string d) : Entry(std::move(l), std::move(d)) {}

    Option(std::string l, std::string d, std::unordered_map<std::string, std::vector<std::string>> a)
        : Entry(std::move(l), std::move(d)), aliases(std::move(a)) {}

    Option() = default;
};

struct Optional : Option {
    std::optional<std::string> default_value;

    Optional(std::string l, std::string d) : Option(std::move(l), std::move(d)) {}

    Optional(std::string l, std::string d, std::optional<std::string> a)
        : Option(std::move(l), std::move(d)), default_value(std::move(a)) {}

    Optional(std::string l, std::string d, std::unordered_map<std::string, std::vector<std::string>> a)
        : Option(std::move(l), std::move(d), std::move(a)) {}

    Optional(std::string l, std::string d, std::unordered_map<std::string, std::vector<std::string>> a, std::optional<std::string> b)
        : Option(std::move(l), std::move(d), std::move(a)), default_value(std::move(b)) {}

    Optional() = default;
};

struct GroupedOptions {
    std::string title;
    std::vector<Optional> items;

    GroupedOptions(std::string t, std::vector<Optional> i)
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
    std::vector<Positional> positionals;
    std::vector<Flag> optional_flags, required_flags;
    std::vector<Optional> optional;
    std::vector<Option> required;
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
    static constexpr std::string_view binName_ = "vh";

    [[nodiscard]] std::string buildSynopsis_() const;
    [[nodiscard]] static std::string normalizePositional_(const std::string& s);
    [[nodiscard]] std::string constructCmdString() const;
    [[nodiscard]] std::string joinAliasesInline_(const std::string& sep = " | ") const;
    [[nodiscard]] std::string joinAliasesCode_();
    void emitCommand(std::ostringstream& out,
        const std::shared_ptr<CommandUsage>& command = nullptr,
        bool spaceLines = false) const;

    [[nodiscard]] std::vector<const CommandUsage*> lineage_() const;
    [[nodiscard]] std::string tokenFor_(const CommandUsage* node) const;
    static bool sameAliases_(const CommandUsage* a, const CommandUsage* b);
    [[nodiscard]] static std::string join_(const std::vector<std::string>& v, std::string_view sep);
    [[nodiscard]] static std::string renderEntryPrimary_(const Entry& e, bool squareIfOptional);
    static void appendWrapped_(std::ostringstream& out,
                               const std::string& text,
                               size_t width,
                               size_t indentAfterFirst = 0);
};

}

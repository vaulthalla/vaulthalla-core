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

    Entry(std::string label_, std::string desc_)
        : label(std::move(label_)), desc(std::move(desc_)) {}

    Entry() = default;
};

struct Positional : Entry {
    std::vector<std::string> aliases;

    Positional(std::string label_, std::string desc_)
        : Entry(std::move(label_), std::move(desc_)) {}

    Positional(std::string label_, std::string desc_, std::vector<std::string> aliases_)
        : Entry(std::move(label_), std::move(desc_)), aliases(std::move(aliases_)) {}

    Positional() = default;

    // 🏗️ Factory Methods

    static Positional Same(const std::string& label, std::string desc) {
        return {label, std::move(desc), {label}};
    }

    // No aliases
    static Positional Required(std::string label, std::string desc) {
        return {std::move(label), std::move(desc)};
    }

    // With aliases
    static Positional WithAliases(std::string label, std::string desc, std::vector<std::string> aliases) {
        return {std::move(label), std::move(desc), std::move(aliases)};
    }

    // Shortcut for one alias
    static Positional Alias(std::string label, std::string desc, std::string alias) {
        return Positional(std::move(label), std::move(desc), {std::move(alias)});
    }
};

struct Flag : Positional {
    bool default_state = false;

    Flag(std::string label_, std::string desc_)
        : Positional(std::move(label_), std::move(desc_)) {}

    Flag(std::string label_, std::string desc_, std::string alias_, const bool default_state_ = false)
        : Positional(std::move(label_), std::move(desc_), std::vector{std::move(alias_)}), default_state(default_state_) {}

    Flag(std::string label_, std::string desc_, std::vector<std::string> aliases_, const bool default_state_)
        : Positional(std::move(label_), std::move(desc_), std::move(aliases_)), default_state(default_state_) {}

    Flag() = default;

    // 🏗️ Factory Methods

    static Flag On(std::string label, std::string desc, std::vector<std::string> aliases = {}) {
        return {std::move(label), std::move(desc), std::move(aliases), true};
    }

    static Flag Off(std::string label, std::string desc, std::vector<std::string> aliases = {}) {
        return {std::move(label), std::move(desc), std::move(aliases), false};
    }

    static Flag Alias(std::string label, std::string desc, std::string alias, const bool default_state = false) {
        return Flag(std::move(label), std::move(desc), std::vector{std::move(alias)}, default_state);
    }

    static Flag WithAliases(std::string label, std::string desc, std::vector<std::string> aliases, const bool default_state = false) {
        return {std::move(label), std::move(desc), std::move(aliases), default_state};
    }

    static Flag Toggle(std::string label, std::string desc, bool default_state) {
        return {std::move(label), std::move(desc), std::vector<std::string>{}, default_state};
    }
};

struct Option : Entry {
    std::vector<std::string> option_tokens{}, value_tokens{};

    Option(std::string label_, std::string desc_,
           std::vector<std::string> option_tokens_,
           std::vector<std::string> value_tokens_)
        : Entry(std::move(label_), std::move(desc_)),
          option_tokens(std::move(option_tokens_)),
          value_tokens(std::move(value_tokens_)) {}

    Option(std::string label_, std::string desc_)
        : Entry(std::move(label_), std::move(desc_)) {}

    Option() = default;

    static Option Single(std::string label,
                         std::string desc,
                         std::string option,
                         std::string value_token) {
        return Option(std::move(label), std::move(desc),
                      {std::move(option)}, {std::move(value_token)});
    }

    static Option Multi(std::string label,
                        std::string desc,
                        std::vector<std::string> options,
                        std::vector<std::string> value_tokens) {
        return Option(std::move(label), std::move(desc),
                      std::move(options), std::move(value_tokens));
    }

    static Option Mirrored(std::string label,
                           std::string desc,
                        const std::string& token) {
        return Option(std::move(label), std::move(desc),
                      {token}, {token});
    }

    static Option OneToMany(std::string label,
                             std::string desc,
                             std::string option,
                             std::vector<std::string> value_tokens) {
        return Option(std::move(label), std::move(desc),
                      {std::move(option)}, std::move(value_tokens));
    }

    static Option Same(std::string token,
                           std::string desc) {
        return Option(token, std::move(desc), {token}, {token});
    }
};

struct Optional : Option {
    std::optional<std::string> default_value{};

    Optional(std::string label_,
             std::string desc_,
             std::vector<std::string> option_tokens_,
             std::vector<std::string> value_tokens_,
             std::optional<std::string> default_value_)
        : Option(std::move(label_), std::move(desc_),
                 std::move(option_tokens_), std::move(value_tokens_)),
          default_value(std::move(default_value_)) {}

    Optional() = default;

    static Optional Single(std::string label,
                           std::string desc,
                           std::string option,
                           std::string value_token,
                           std::optional<std::string> def = std::nullopt) {
        return Optional(std::move(label), std::move(desc),
                        {std::move(option)}, {std::move(value_token)},
                        std::move(def));
    }

    static Optional Multi(std::string label,
                          std::string desc,
                          std::vector<std::string> options,
                          std::vector<std::string> value_tokens,
                          std::optional<std::string> def = std::nullopt) {
        return {std::move(label), std::move(desc),
                        std::move(options), std::move(value_tokens),
                        std::move(def)};
    }

    static Optional OneToMany(std::string label,
                                 std::string desc,
                                 std::string option,
                                 std::vector<std::string> value_tokens,
                                 std::optional<std::string> def = std::nullopt) {
        return Optional(std::move(label), std::move(desc),
                        {std::move(option)}, std::move(value_tokens),
                        std::move(def));
    }

    static Optional ManyToOne(std::string label,
                             std::string desc,
                             std::vector<std::string> options,
                             std::string value_token,
                             std::optional<std::string> def = std::nullopt) {
        return Optional(std::move(label), std::move(desc),
                        std::move(options), {std::move(value_token)},
                        std::move(def));
    }

    static Optional Mirrored(std::string label,
                             std::string desc,
                             std::string token,
                             std::optional<std::string> def = std::nullopt) {
        return Optional(std::move(label), std::move(desc),
                        {token}, {token}, std::move(def));
    }

    // 🆕 new factory
    static Optional Same(std::string token,
                         std::string desc,
                         std::optional<std::string> def = std::nullopt) {
        return Optional(token, std::move(desc), {token}, {token}, std::move(def));
    }
};

struct GroupedOptions {
    std::string title;
    std::vector<std::variant<Optional, Flag>> items;

    GroupedOptions(std::string t, std::vector<std::variant<Optional, Flag>> i)
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

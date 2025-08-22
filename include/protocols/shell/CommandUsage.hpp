#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstddef>

namespace vh::shell {

// A simple labeled entry (option/flag), with optional aliases.
struct Entry {
    std::string label;                  // primary, e.g. "--s3"
    std::string desc;                   // description
    std::vector<std::string> aliases;   // e.g. {"-S", "--s3-backend"}
};

// Grouped options section, e.g. "S3 Options"
struct GroupedOptions {
    std::string title;
    std::vector<Entry> items;
};

// Example: {"vh vault sync myvault --s3 --now", "Trigger immediate S3 sync"}
struct Example {
    std::string cmd;
    std::string note;
};

// ANSI color theme (simple, customizable). Set enabled=false to disable.
struct ColorTheme {
    bool enabled = true;

    // Basic defaults: tweak to taste
    std::string header = "\033[1;36m"; // section titles (bold cyan)
    std::string command = "\033[1;32m"; // command name (bold green)
    std::string key = "\033[33m";      // left column keys (yellow)
    std::string dim = "\033[2m";       // dim/secondary text
    std::string reset = "\033[0m";     // reset

    // Convenience helpers
    [[nodiscard]] std::string maybe(const std::string& code) const {
        return enabled ? code : "";
    }
    [[nodiscard]] std::string H() const { return maybe(header); }
    [[nodiscard]] std::string C() const { return maybe(command); }
    [[nodiscard]] std::string K() const { return maybe(key); }
    [[nodiscard]] std::string D() const { return maybe(dim); }
    [[nodiscard]] std::string R() const { return maybe(reset); }
};

class CommandUsage {
public:
    // identity
    std::string ns, command;                         // e.g. "vault sync"
    std::vector<std::string> ns_aliases, command_aliases;    // e.g. {"vsync", "v-sync"}
    std::string description;                     // one-liner or paragraph
    std::optional<std::string> synopsis;         // if empty, synthesized

    // arguments/flags
    std::vector<Entry> positionals;              // ordered; appear in synopsis
    std::vector<Entry> required;                 // two-col section
    std::vector<Entry> optional;                 // two-col section
    std::vector<GroupedOptions> groups;                   // extra grouped sections

    // examples
    std::vector<Example> examples;

    // rendering knobs
    int term_width = 100;                        // target width for toText()
    std::size_t max_key_col = 30;                // cap left column width
    bool show_aliases = true;                    // include aliases in text/MD output
    ColorTheme theme{};                          // ANSI theme (enable to colorize)

    // render for terminal help
    [[nodiscard]] std::string toText() const;

    // render as Markdown (good for pandoc)
    [[nodiscard]] std::string toMarkdown() const;

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

// Optional: stitch multiple commands together
class CommandBook {
public:
    std::string title; // e.g. "Vaulthalla CLI"
    std::vector<CommandUsage> commands;

    // If set, overrides each command's theme (useful for uniform coloring)
    std::optional<ColorTheme> book_theme;

    [[nodiscard]] std::string toText() const;
    [[nodiscard]] std::string toMarkdown() const;
};

} // namespace vh::shell

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <initializer_list>
#include <nlohmann/json.hpp>

namespace vh::shell {

struct CommandCall;

struct CommandResult {
    int exit_code = 0;                 // 0 = success
    std::string stdout_text;           // CLI stdout
    std::string stderr_text;           // CLI stderr
    nlohmann::json data;               // optional machine-readable payload
    bool has_data = false;
};

using CommandHandler = std::function<CommandResult(const CommandCall&)>;

struct CommandInfo {
    std::string description;                 // own
    CommandHandler handler;
    std::unordered_set<std::string> aliases; // own normalized aliases (no dashes)
    void print(std::string_view canonical) const;
};

class Router {
public:
    // Views in, storage owns copies
    void registerCommand(std::string_view name,
                         std::string_view desc,
                         CommandHandler handler,
                         std::initializer_list<std::string_view> aliases = {});
    CommandResult execute(const CommandCall& call) const;

    // Accept a view; we'll copy only once if needed
    CommandResult executeLine(std::string_view line) const;

    std::string listCommands() const;

private:
    // Own the canonical command entries and alias strings
    std::unordered_map<std::string, CommandInfo> commands_;
    std::unordered_map<std::string, std::string> aliasMap_; // alias -> canonical

    std::string canonicalFor(std::string_view nameOrAlias) const;

    // Normalizers: views in, lowercase copies out (owned)
    static std::string normalize(std::string_view s);
    static std::string joinAliases(const std::unordered_set<std::string>& aliases);
    static std::string strip_leading_dashes(std::string_view s);
    static std::string normalize_alias(std::string_view s);
    static std::string pretty_alias(std::string_view a);
};

} // namespace vh::shell

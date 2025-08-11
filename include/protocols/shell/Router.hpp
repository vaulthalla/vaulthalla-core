#pragma once

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <nlohmann/json.hpp>

namespace vh::shell {

struct CommandCall;

struct CommandResult {
    int exit_code = 0;                 // 0 = success
    std::string stdout_text;           // what the CLI should print to stdout
    std::string stderr_text;           // what the CLI should print to stderr
    nlohmann::json data;               // optional machine-readable payload
    bool has_data = false;
};

using CommandHandler = std::function<CommandResult(const CommandCall&)>;

struct CommandInfo {
    std::string description;
    CommandHandler handler;
    std::unordered_set<std::string> aliases;
    void print(const std::string& canonical) const;
};

class Router {
public:
    void registerCommand(const std::string& name, std::string desc, CommandHandler handler,
                         std::initializer_list<std::string> aliases = {});
    CommandResult execute(const CommandCall& call) const;
    CommandResult executeLine(const std::string& line) const;
    std::string listCommands() const;

private:
    std::unordered_map<std::string, CommandInfo> commands_;
    std::unordered_map<std::string, std::string> aliasMap_;

    std::string canonicalFor(const std::string& nameOrAlias) const;

    static std::string normalize(const std::string& s);
    static std::string joinAliases(const std::unordered_set<std::string>& aliases);
    static std::string strip_leading_dashes(const std::string& s);
    static std::string normalize_alias(const std::string& s);
    static std::string pretty_alias(const std::string& a);
    static int term_width();
    static std::string wrap_text(std::string_view s, size_t width);
};

}

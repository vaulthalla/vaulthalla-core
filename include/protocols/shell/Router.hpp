#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <memory>

namespace vh::shell {

struct CommandCall; // from Parser.hpp

using CommandHandler = std::function<void(const CommandCall&)>;

struct CommandInfo {
    std::string description;
    CommandHandler handler;
    std::unordered_set<std::string> aliases;

    // Print with the canonical name for context
    void print(const std::string& canonical) const;
};

class Router : public std::enable_shared_from_this<Router> {
public:
    // Register a command with optional aliases
    void registerCommand(const std::string& name, std::string desc, CommandHandler handler,
                         std::initializer_list<std::string> aliases = {});

    // New: one-shot from a full input line (tokenize -> parse -> dispatch)
    void executeLine(const std::string& line) const;

    // Execute using a parsed call directly
    void execute(const CommandCall& call) const;

    // Legacy: execute with name + positionals only (flags ignored)
    void execute(const std::string& name, const std::vector<std::string>& args) const;

    // List all registered commands and aliases
    void listCommands() const;

    static std::string joinAliases(const std::unordered_set<std::string>& aliases);

private:
    std::unordered_map<std::string, CommandInfo> commands_;
    std::unordered_map<std::string, std::string> aliasMap_; // alias -> canonical name

    static std::string normalize(const std::string& s);

    // alias or name -> canonical
    std::string canonicalFor(const std::string& nameOrAlias) const;

    // In Router.hpp / .cpp
    static std::string strip_leading_dashes(const std::string& s);

    static std::string normalize_alias(const std::string& s);
};

}

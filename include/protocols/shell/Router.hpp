#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

namespace vh::shell {

using CommandHandler = std::function<void(const std::vector<std::string>& args)>;

struct CommandInfo {
    std::string description;
    CommandHandler handler;
    std::unordered_set<std::string> aliases;
};

class Router {
public:
    // Register a command with optional aliases
    void registerCommand(std::string name, std::string desc, CommandHandler handler,
                         std::initializer_list<std::string> aliases = {});

    // Execute a command (or alias) by name
    void execute(const std::string& name, const std::vector<std::string>& args) const;

    // List all registered commands and aliases
    void listCommands() const;

private:
    std::unordered_map<std::string, CommandInfo> commands_;
    std::unordered_map<std::string, std::string> aliasMap_; // alias -> canonical name

    static std::string normalize(const std::string& s);
    static std::string joinAliases(const std::unordered_set<std::string>& aliases);
};

} // namespace vh::shell

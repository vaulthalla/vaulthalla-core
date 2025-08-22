#pragma once

#include <protocols/shell/types.hpp>

#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <initializer_list>
#include <nlohmann/json.hpp>

namespace vh::types {
    struct User;
}

namespace vh::shell {

class CommandUsage;

class Router {
public:
    void registerCommand(const CommandUsage& usage, CommandHandler handler);
    CommandResult execute(const CommandCall& call) const;

    // Accept a view; we'll copy only once if needed
    CommandResult executeLine(const std::string& line, const std::shared_ptr<types::User>& user) const;

private:
    std::unordered_map<std::string, CommandInfo> commands_;
    std::unordered_map<std::string, std::string> aliasMap_; // alias -> canonical

    std::string canonicalFor(const std::string& nameOrAlias) const;

    static std::string normalize(const std::string& s);
    static std::string normalize_alias(const std::string& s);
    static std::string joinAliases(const std::unordered_set<std::string>& aliases);
    static std::string strip_leading_dashes(const std::string& s);
    static std::string pretty_alias(const std::string& a);
};

} // namespace vh::shell

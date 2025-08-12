#pragma once

#include <protocols/shell/types.hpp>

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <initializer_list>
#include <nlohmann/json.hpp>

namespace vh::types {
    struct User;
}

namespace vh::shell {

class Router {
public:
    // Views in, storage owns copies
    void registerCommand(std::string_view name,
                         std::string_view desc,
                         CommandHandler handler,
                         std::initializer_list<std::string_view> aliases = {});
    CommandResult execute(const CommandCall& call) const;

    // Accept a view; we'll copy only once if needed
    CommandResult executeLine(std::string_view line, const std::shared_ptr<types::User>& user) const;

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

#pragma once

#include "protocols/shell/types.hpp"
#include "types/PermissionOverride.hpp"

#include <optional>
#include <string>
#include <vector>
#include <string_view>
#include <utility>
#include <memory>
#include <regex>

namespace vh::types {
struct Role;
}

namespace vh::shell {

template <typename T> struct Lookup {
    std::shared_ptr<T> ptr;
    std::string error;
    explicit operator bool() const { return static_cast<bool>(ptr); }
};

struct PatternParse {
    bool ok = false;
    std::string raw;
    std::optional<std::regex> compiled;
    std::string error;
};

struct EnableParse {
    bool ok = false;
    std::optional<bool> value;
    std::string error;
};

struct EffectParse {
    bool ok = false;
    std::optional<types::OverrideOpt> value;
    std::string error;
};

struct Subject {
    std::string type; // "user" or "group"
    unsigned int id = 0;
};

CommandResult invalid(std::string msg);
CommandResult invalid(const std::vector<std::string>& args, std::string msg);
CommandResult ok(std::string out);
CommandResult usage(const std::vector<std::string>& args = {});

std::optional<std::string> optVal(const CommandCall& c, const std::string& key);

std::optional<int> parseInt(const std::string& sv);

uintmax_t parseSize(const std::string& s);

[[nodiscard]] bool hasFlag(const CommandCall& c, const std::string& key);
[[nodiscard]] bool hasKey(const CommandCall& c, const std::string& key);

bool isCommandMatch(const std::vector<std::string>& path, std::string_view subcmd);

Lookup<Subject> parseSubject(const CommandCall& call, const std::string& errPrefix);
Lookup<types::Role> resolveRole(const std::string& roleArg, const std::string& errPrefix);

std::pair<std::string_view, CommandCall> descend(const CommandCall& call);

}

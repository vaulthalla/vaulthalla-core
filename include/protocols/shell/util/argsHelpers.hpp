#pragma once

#include "protocols/shell/types.hpp"
#include "rbac/model/PermissionOverride.hpp"
#include "database/model/ListQueryParams.hpp"

#include <optional>
#include <string>
#include <vector>
#include <string_view>
#include <utility>
#include <memory>
#include <regex>

namespace vh::rbac::model { struct Role; }
namespace vh::identities::model { struct User; }

namespace vh::protocols::shell {

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
    std::optional<rbac::model::OverrideOpt> value;
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

std::optional<std::string> optVal(const CommandCall& c, const std::vector<std::string>& args);
std::optional<std::string> optVal(const CommandCall& c, const std::string& key);

std::optional<unsigned int> parseUInt(const std::string& sv);

uintmax_t parseSize(const std::string& s);

[[nodiscard]] bool hasFlag(const CommandCall& c, const std::string& key);
[[nodiscard]] bool hasFlag(const CommandCall& c, const std::vector<std::string>& keys);

[[nodiscard]] bool hasKey(const CommandCall& c, const std::string& key);

bool isCommandMatch(const std::vector<std::string>& path, std::string_view subcmd);

Lookup<Subject> parseSubject(const CommandCall& call, const std::string& errPrefix);
Lookup<rbac::model::Role> resolveRole(const std::string& roleArg, const std::string& errPrefix);
Lookup<identities::model::User> resolveUser(const std::string& userArg, const std::string& errPrefix);

std::pair<std::string_view, CommandCall> descend(const CommandCall& call);

database::model::ListQueryParams parseListQuery(const CommandCall& call);

}

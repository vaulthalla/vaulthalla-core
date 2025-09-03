#include "util/shellArgsHelpers.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "usage/include/CommandUsage.hpp"
#include "usage/include/UsageManager.hpp"
#include "database/Queries/UserQueries.hpp"
#include "database/Queries/GroupQueries.hpp"
#include "database/Queries/PermsQueries.hpp"
#include "types/User.hpp"
#include "types/Group.hpp"
#include "types/Role.hpp"

#include <optional>
#include <string>
#include <cctype>
#include <algorithm>

using namespace vh::shell;
using namespace vh::services;
using namespace vh::types;
using namespace vh::database;

CommandResult vh::shell::invalid(std::string msg) { return {2, "", std::move(msg)}; }
CommandResult vh::shell::ok(std::string out) { return {0, std::move(out), ""}; }

CommandResult vh::shell::invalid(const std::vector<std::string>& args, std::string msg) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;
    return {2, usageManager->renderHelp(args), std::move(msg)};
}

CommandResult vh::shell::usage(const std::vector<std::string>& args) {
    const auto usageManager = ServiceDepsRegistry::instance().shellUsageManager;
    return {0, usageManager->renderHelp(args), ""};
}

std::optional<std::string> vh::shell::optVal(const CommandCall& c, const std::string& key) {
    for (const auto& [k, v] : c.options) if (k == key) return v.value_or(std::string{});
    return std::nullopt;
}

bool vh::shell::hasFlag(const CommandCall& c, const std::string& key) {
    for (const auto& [k, v] : c.options) if (k == key) return !v.has_value();
    return false;
}
bool vh::shell::hasKey(const CommandCall& c, const std::string& key) {
    return std::ranges::any_of(c.options, [&key](const auto& kv) { return kv.key == key; });
}

std::optional<int> vh::shell::parseInt(const std::string& sv) {
    if (sv.empty()) return std::nullopt;
    int v = 0; bool neg = false; size_t i = 0;
    if (sv[0] == '-') { neg = true; i = 1; }
    for (; i < sv.size(); ++i) {
        char c = sv[i];
        if (c < '0' || c > '9') return std::nullopt;
        v = v*10 + (c - '0');
    }
    return neg ? -v : v;
}

uintmax_t vh::shell::parseSize(const std::string& s) {
    switch (std::toupper(s.back())) {
    case 'T': return std::stoull(s.substr(0, s.size() - 1)) * 1024 * 1024 * 1024 * 1024; // TiB
    case 'G': return std::stoull(s.substr(0, s.size() - 1)) * 1024;                      // GiB
    case 'M': return std::stoull(s.substr(0, s.size() - 1)) * 1024 * 1024;               // MiB
    default: return std::stoull(s);                                                      // Assume bytes if no suffix
    }
}

bool vh::shell::isCommandMatch(const std::vector<std::string>& path, std::string_view subcmd) {
    const auto usage = ServiceDepsRegistry::instance().shellUsageManager->resolve(path);
    return std::ranges::any_of(usage->aliases, [&](const auto& alias) { return alias == subcmd; });
}

std::pair<std::string_view, CommandCall> vh::shell::descend(const CommandCall& call) {
    if (call.positionals.empty()) return {"", call};
    std::string_view sub = call.positionals[0];
    CommandCall subcall = call;
    subcall.positionals.erase(subcall.positionals.begin());
    return {sub, subcall};
}

Lookup<Subject> vh::shell::parseSubject(const CommandCall& call, const std::string& errPrefix) {
    Lookup<Subject> out;

    const std::vector<std::string> userFlags = {"user", "u"};
    for (const auto& f : userFlags) {
        if (const auto v = optVal(call, f)) {
            if (const auto idOpt = parseInt(*v)) {
                if (*idOpt <= 0) { out.error = errPrefix + ": user ID must be a positive integer"; return out; }
                out.ptr = std::make_shared<Subject>(Subject{"user", static_cast<unsigned int>(*idOpt)});
                return out;
            }
            auto user = UserQueries::getUserByName(*v);
            if (!user) { out.error = errPrefix + ": user not found: " + *v; return out; }
            out.ptr = std::make_shared<Subject>(Subject{"user", static_cast<unsigned int>(user->id)});
            return out;
        }
    }

    const std::vector<std::string> groupFlags = {"group", "g"};
    for (const auto& f : groupFlags) {
        if (const auto v = optVal(call, f)) {
            if (const auto idOpt = parseInt(*v)) {
                if (*idOpt <= 0) { out.error = errPrefix + ": group ID must be a positive integer"; return out; }
                out.ptr = std::make_shared<Subject>(Subject{"group", static_cast<unsigned int>(*idOpt)});
                return out;
            }
            auto group = GroupQueries::getGroupByName(*v);
            if (!group) { out.error = errPrefix + ": group not found: " + *v; return out; }
            out.ptr = std::make_shared<Subject>(Subject{"group", group->id});
            return out;
        }
    }

    out.error = errPrefix + ": must specify either --user/-u or --group/-g";
    return out;
}

Lookup<Role> vh::shell::resolveRole(const std::string& roleArg, const std::string& errPrefix) {
    Lookup<Role> out;

    if (const auto roleIdOpt = parseInt(roleArg)) {
        if (*roleIdOpt <= 0) {
            out.error = errPrefix + ": role ID must be a positive integer";
            return out;
        }
        out.ptr = PermsQueries::getRole(*roleIdOpt);
    } else out.ptr = PermsQueries::getRoleByName(roleArg);

    if (!out.ptr) out.error = errPrefix + ": role not found";
    return out;
}

ListQueryParams vh::shell::parseListQuery(const CommandCall& call) {
    ListQueryParams p;

    if (const auto sortOpt = optVal(call, "sort")) {
        if (sortOpt->empty()) throw std::invalid_argument("Invalid --sort value: cannot be empty");
        p.sort = *sortOpt;
    }

    if (const auto dirOpt = optVal(call, "direction")) {
        if (dirOpt->empty()) throw std::invalid_argument("Invalid --direction value: cannot be empty");
        const auto dirLower = [&]() {
            std::string s = *dirOpt;
            std::ranges::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
            return s;
        }();
        if (dirLower == "asc") p.direction = SortDirection::ASC;
        else if (dirLower == "desc") p.direction = SortDirection::DESC;
        else throw std::invalid_argument("Invalid --direction value: must be 'asc' or 'desc'");
    }

    if (const auto filterOpt = optVal(call, "filter")) {
        if (filterOpt->empty()) throw std::invalid_argument("Invalid --filter value: cannot be empty");
        p.filter = *filterOpt;
    }

    if (const auto limitOpt = optVal(call, "limit")) {
        if (limitOpt->empty()) throw std::invalid_argument("Invalid --limit value: cannot be empty");
        const auto limitParsed = parseInt(*limitOpt);
        if (!limitParsed || *limitParsed <= 0)
            throw std::invalid_argument("Invalid --limit value: must be a positive integer");
        p.limit = *limitParsed;
    }

    if (const auto pageOpt = optVal(call, "page")) {
        if (pageOpt->empty()) throw std::invalid_argument("Invalid --page value: cannot be empty");
        const auto pageParsed = parseInt(*pageOpt);
        if (!pageParsed || *pageParsed <= 0)
            throw std::invalid_argument("Invalid --page value: must be a positive integer");
        p.page = *pageParsed;
    }

    return p;
}

Lookup<User> vh::shell::resolveUser(const std::string& userArg, const std::string& errPrefix) {
    Lookup<User> out;

    if (const auto idOpt = parseInt(userArg)) {
        if (*idOpt <= 0) {
            out.error = errPrefix + ": user ID must be a positive integer";
            return out;
        }
        out.ptr = UserQueries::getUserById(*idOpt);
    } else out.ptr = UserQueries::getUserByName(userArg);
}

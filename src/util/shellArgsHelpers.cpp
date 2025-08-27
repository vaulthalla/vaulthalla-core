#include "util/shellArgsHelpers.hpp"

#include <optional>
#include <string>
#include <cctype>
#include <algorithm>

using namespace vh::shell;

CommandResult vh::shell::invalid(std::string msg) { return {2, "", std::move(msg)}; }
CommandResult vh::shell::ok(std::string out) { return {0, std::move(out), ""}; }

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

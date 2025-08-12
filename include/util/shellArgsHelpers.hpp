#pragma once

#include "protocols/shell/types.hpp"

#include <string_view>
#include <optional>

namespace vh::shell {

inline CommandResult invalid(std::string msg) { return {2, "", std::move(msg)}; }
inline CommandResult ok(std::string out) { return {0, std::move(out), ""}; }

inline std::optional<std::string_view> optVal(const CommandCall& c, const std::string_view key) {
    for (const auto& [k, v] : c.options) if (k == key) return v.value_or(std::string_view{});
    return std::nullopt;
}
inline bool hasFlag(const CommandCall& c, const std::string_view key) {
    for (const auto& [k, v] : c.options) if (k == key) return !v.has_value();
    return false;
}
inline bool hasKey(const CommandCall& c, const std::string_view key) {
    return std::ranges::any_of(c.options, [&key](const auto& kv) { return kv.key == key; });
}
inline std::optional<int> parseInt(const std::string_view sv) {
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

}

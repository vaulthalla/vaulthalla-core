#pragma once

#include "protocols/shell/Token.hpp"
#include "protocols/shell/types.hpp"

#include <string_view>
#include <string>
#include <vector>
#include <optional>
#include <cctype>

namespace vh::shell {

// Own a string and return a stable view into it
inline std::string_view own(CommandCall& c, std::string s) {
    c.arena.emplace_back(std::move(s));
    return std::string_view(c.arena.back());
}

// Upsert a flag (last wins)
inline void setOpt(CommandCall& c,
                   std::string_view key,
                   std::optional<std::string_view> val) {
    for (auto& kv : c.options) if (kv.key == key) { kv.value = val; return; }
    c.options.push_back(FlagKV{key, val});
}

static inline bool looks_number(std::string_view s) {
    if (s.empty()) return false;
    size_t i = 0;
    if (s[i] == '-') { if (s.size()==1) return false; ++i; }
    bool dot=false, digit=false;
    for (; i<s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (std::isdigit(c)) { digit=true; continue; }
        if (s[i]=='.' && !dot) { dot=true; continue; }
        return false;
    }
    return digit;
}

inline CommandCall parseTokens(const std::vector<Token>& toks) {
    CommandCall call;
    call.options.reserve(8);
    call.positionals.reserve(8);

    // 1) Command name = first Word
    size_t i = 0;
    for (; i < toks.size(); ++i) {
        if (toks[i].type == TokenType::Word) {
            call.name = toks[i].text;
            ++i;
            break;
        }
    }
    // Optional: if no Word found, allow leading help alias as "name"
    if (call.name.empty() && !toks.empty() && toks[0].type == TokenType::Flag) {
        // Only if you explicitly allow this (e.g., "help"/"h" came in as a Flag)
        call.name = toks[0].text;
        i = 1;
    }

    bool stop_flags = false;

    auto push_flag = [&](std::string_view k, std::optional<std::string_view> v) {
        call.options.push_back(FlagKV{ k, v });
    };

    for (; i < toks.size(); ++i) {
        const Token& t = toks[i];

        // Sentinel: "--" arrives as a Word from the tokenizer
        if (!stop_flags && t.type == TokenType::Word && t.text == "--") {
            stop_flags = true;
            continue;
        }

        if (!stop_flags && t.type == TokenType::Flag) {
            // Try to consume a following value if the next token is a Word
            if (i + 1 < toks.size() && toks[i+1].type == TokenType::Word) {
                // Accept negative numbers as values too
                push_flag(t.text, toks[i+1].text);
                ++i;
            } else {
                // Boolean flag
                push_flag(t.text, std::nullopt);
            }
            continue;
        }

        // Positional (either after "--" or just a Word)
        call.positionals.push_back(t.text);
    }

    return call;
}

}

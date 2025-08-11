#pragma once
#include <string>
#include <vector>
#include <string_view>

namespace vh::shell {

enum class TokenType { Word, Flag, Value }; // keep Value available if you want later

struct Token {
    TokenType type;
    std::string text; // owning; keep it simple
};

static inline bool looks_negative_number(std::string_view s) {
    if (s.size() < 2 || s[0] != '-') return false;
    bool dot = false, digit = false;
    for (size_t i = 1; i < s.size(); ++i) {
        char c = s[i];
        if (c >= '0' && c <= '9') { digit = true; continue; }
        if (c == '.' && !dot) { dot = true; continue; }
        return false;
    }
    return digit;
}

// Expand short bundles: "-abc" -> "-a" "-b" "-c"
inline void expandShortBundle(const std::string& word, std::vector<Token>& out) {
    // word starts with '-' and word.size()>2 and not "-k=value"
    // We generate Flag tokens with names without dashes ("a","b","c")
    for (size_t i = 1; i < word.size(); ++i) {
        if (word[i] == '=') {
            // Treat "-a=val" as "-a" "val"
            out.push_back({TokenType::Flag, std::string(1, word[1])});
            out.push_back({TokenType::Word, word.substr(i+1)});
            return;
        }
        out.push_back({TokenType::Flag, std::string(1, word[i])});
    }
}

// Push a single token or split key=value; strips dashes from Flag token text
inline void pushFlagOrWord(std::vector<Token>& tokens, const std::string& word) {
    if (word == "--") {
        // Represent sentinel as a Word so parseTokens can flip into positional-only
        tokens.push_back({TokenType::Word, "--"});
        return;
    }

    if (word.rfind("--", 0) == 0) {
        // Long flag: --key or --key=value
        auto eq = word.find('=');
        if (eq != std::string::npos) {
            tokens.push_back({TokenType::Flag, word.substr(2, eq - 2)}); // key w/o dashes
            tokens.push_back({TokenType::Word, word.substr(eq + 1)});    // value
        } else {
            tokens.push_back({TokenType::Flag, word.substr(2)});
        }
        return;
    }

    if (word.size() > 1 && word[0] == '-') {
        // Negative numbers should be words, not flags
        if (looks_negative_number(word)) {
            tokens.push_back({TokenType::Word, word});
            return;
        }

        // Short flag: -k, -k=value, -abc bundle, or -pVALUE (handled later)
        auto eq = word.find('=');
        if (eq != std::string::npos) {
            tokens.push_back({TokenType::Flag, word.substr(1, eq - 1)});
            tokens.push_back({TokenType::Word, word.substr(eq + 1)});
            return;
        }

        if (word.size() > 2) {
            // Could be bundle or -pVALUE; we emit bundle of flags here.
            // If -p takes a value and was glued (e.g., -pVALUE), your validator can reattach VALUE.
            expandShortBundle(word, tokens);
            return;
        }

        // Simple short flag: -k
        tokens.push_back({TokenType::Flag, word.substr(1)});
        return;
    }

    // Plain word
    tokens.push_back({TokenType::Word, word});
}

// Tokenize with basic quoting and \-escapes (spaces inside quotes preserved)
inline std::vector<Token> tokenize(const std::string& line) {
    std::vector<Token> tokens;
    tokens.reserve(16);

    const char* s = line.c_str();
    const char* e = s + line.size();
    const char* p = s;

    auto skip_ws = [&](const char*& q){ while (q < e && (*q == ' ' || *q == '\t')) ++q; };

    skip_ws(p);
    while (p < e) {
        if (*p == '"') { // double-quoted
            ++p;
            std::string buf;
            buf.reserve(32);
            while (p < e && *p != '"') {
                if (*p == '\\' && p + 1 < e) { ++p; } // keep next char verbatim
                buf.push_back(*p++);
            }
            if (p < e && *p == '"') ++p;
            // Quoted token is always a Word (could be a value to a flag)
            tokens.push_back({TokenType::Word, std::move(buf)});
        } else if (*p == '\'') { // single-quoted
            ++p;
            std::string buf;
            buf.reserve(32);
            while (p < e && *p != '\'') buf.push_back(*p++);
            if (p < e && *p == '\'') ++p;
            tokens.push_back({TokenType::Word, std::move(buf)});
        } else {
            // unquoted: read until space
            const char* b = p;
            while (p < e && *p != ' ' && *p != '\t' && *p != '"' && *p != '\'') ++p;
            std::string word(b, p - b);
            pushFlagOrWord(tokens, word);
        }
        skip_ws(p);
    }

    return tokens;
}

}

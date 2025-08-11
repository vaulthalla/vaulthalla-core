#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace vh::shell {

enum class TokenType { Word, Flag, Value }; // Value remains available

struct Token {
    TokenType type;
    std::string text;
};

inline void pushFlagOrWord(std::vector<Token>& tokens, std::string word) {
    if (word.starts_with("--")) {
        // --key=value or --key
        auto eq = word.find('=');
        if (eq != std::string::npos) {
            tokens.push_back({TokenType::Flag, word.substr(2, eq - 2)});
            tokens.push_back({TokenType::Word, word.substr(eq + 1)});
        } else {
            tokens.push_back({TokenType::Flag, word.substr(2)});
        }
    } else if (word.starts_with("-") && word.size() > 1) {
        // -k=value or -k  (no short-flag grouping here; keep it simple)
        auto eq = word.find('=');
        if (eq != std::string::npos) {
            tokens.push_back({TokenType::Flag, word.substr(1, eq - 1)});
            tokens.push_back({TokenType::Word, word.substr(eq + 1)});
        } else {
            tokens.push_back({TokenType::Flag, word.substr(1)});
        }
    } else {
        tokens.push_back({TokenType::Word, word});
    }
}

inline std::vector<Token> tokenize(const std::string& line) {
    std::vector<Token> tokens;
    std::istringstream iss(line);
    std::string word;
    while (iss >> word) pushFlagOrWord(tokens, word);
    return tokens;
}

}

#pragma once

#include <string>
#include <vector>
#include <sstream>

namespace vh::shell {

enum class TokenType { Word, Flag, Value };

struct Token {
    TokenType type;
    std::string text;
};

inline std::vector<Token> tokenize(const std::string& line) {
    std::vector<Token> tokens;
    std::istringstream iss(line);
    std::string word;
    while (iss >> word) {
        if (word.starts_with("--")) tokens.push_back({TokenType::Flag, word.substr(2)});
        else if (word.starts_with("-")) tokens.push_back({TokenType::Flag, word.substr(1)});
        else tokens.push_back({TokenType::Word, word});
    }
    return tokens;
}

}

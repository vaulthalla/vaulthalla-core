#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include "protocols/shell/Token.hpp"

namespace vh::shell {

struct CommandCall {
    std::string name;
    std::unordered_map<std::string, std::string> options;
    std::vector<std::string> positionals;
};

inline CommandCall parseTokens(const std::vector<Token>& tokens) {
    CommandCall call;
    size_t i = 0;

    // First non-flag is the command name
    for (; i < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::Word) {
            call.name = tokens[i].text;
            ++i;
            break;
        }
    }

    // Parse rest (flags + positionals)
    for (; i < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::Flag) {
            std::string key = tokens[i].text;
            std::string val;
            if (i + 1 < tokens.size() && tokens[i+1].type == TokenType::Word) {
                val = tokens[++i].text;
            }
            call.options[key] = val;
        } else {
            call.positionals.push_back(tokens[i].text);
        }
    }

    return call;
}

}

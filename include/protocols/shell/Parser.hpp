#pragma once

#include "protocols/shell/Token.hpp"
#include "logging/LogRegistry.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace vh::shell {

using namespace logging;

struct CommandCall {
    std::string name;
    std::unordered_map<std::string, std::string> options;
    std::vector<std::string> positionals;
};

inline CommandCall parseTokens(const std::vector<Token>& tokens) {
    CommandCall call;
    size_t i = 0;

    // Prefer a real word as the command name
    for (; i < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::Word) {
            call.name = tokens[i].text;
            ++i;
            break;
        }
    }

    // If no word found, allow a single leading flag to be the command name
    if (call.name.empty() && !tokens.empty() && tokens[0].type == TokenType::Flag) {
        call.name = tokens[0].text; // "h" or "help" (no dashes)
        i = 1; // parse the rest as normal
    }

    // Parse rest (flags + positionals)
    for (; i < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::Flag) {
            std::string key = tokens[i].text;
            std::string val;
            if (i + 1 < tokens.size() && tokens[i+1].type == TokenType::Word) val = tokens[++i].text;
            call.options[key] = val;
        } else {
            call.positionals.push_back(tokens[i].text);
        }
    }
    return call;
}

}

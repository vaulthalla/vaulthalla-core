#pragma once

#include <set>
#include <string>
#include <string>
#include <cctype>

namespace vh::database {

inline std::string toSnakeCase(const std::string& input) {
    std::string out;
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];

        if (std::isspace(c) || c == '-' || c == '.' || c == '/') {
            out += '_';
        }
        else if (std::isupper(c)) {
            if (i > 0 && (std::islower(input[i - 1]) || std::isdigit(input[i - 1]) || input[i - 1] == '_')) {
                out += '_';
            }
            out += std::tolower(c);
        }
        else {
            out += c;
        }
    }
    return out;
}


}

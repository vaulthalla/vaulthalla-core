#pragma once

#include <string>
#include <vector>

namespace vh::rbac::glob::model {

struct Token;

struct Pattern {
    std::string source;
    std::vector<Token> tokens;
};

}

#pragma once

#include "rbac/glob/model/Pattern.hpp"

#include <string>

namespace vh::rbac::glob {

struct Tokenizer {
    static model::Pattern parse(const std::string& pattern);
    static void validate(const std::string& pattern);
};

}

#pragma once

#include <string>

namespace vh::rbac::glob::model {

struct Token {
    enum class Type {
        Literal,
        Star,
        DoubleStar,
        Question,
        Slash
    };

    Type type;
    std::string value;
};

}

#pragma once

#include <string>

namespace vh::protocols::shell {

struct ExecResult {
    int code = 1;
    std::string output;
};

}

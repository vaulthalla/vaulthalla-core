#pragma once

#include <string>

namespace vh::shell {

struct ColorTheme {
    bool enabled = true;

    std::string header = "\033[1;36m"; // section titles (bold cyan)
    std::string command = "\033[1;32m"; // command name (bold green)
    std::string key = "\033[33m";      // left column keys (yellow)
    std::string dim = "\033[2m";       // dim/secondary text
    std::string reset = "\033[0m";

    [[nodiscard]] std::string maybe(const std::string& code) const { return enabled ? code : ""; }
    [[nodiscard]] std::string H() const { return maybe(header); }
    [[nodiscard]] std::string C() const { return maybe(command); }
    [[nodiscard]] std::string K() const { return maybe(key); }
    [[nodiscard]] std::string D() const { return maybe(dim); }
    [[nodiscard]] std::string R() const { return maybe(reset); }
};

}

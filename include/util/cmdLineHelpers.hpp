#pragma once

#include <sys/ioctl.h>
#include <unistd.h>
#include <string>
#include <string_view>
#include <fmt/core.h>

namespace vh::shell {

inline int term_width() {
    if (!isatty(STDOUT_FILENO)) return 80;
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return ws.ws_col;
    const char* c = std::getenv("COLUMNS");
    if (c) { int n = std::atoi(c); if (n > 0) return n; }
    return 80;
}

inline std::string wrap_text(std::string_view s, size_t width) {
    std::string out; size_t col = 0;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = i;
        while (j < s.size() && s[j] != ' ') ++j;
        size_t wlen = j - i;
        if (col && col + 1 + wlen > width) { out += '\n'; col = 0; }
        if (col) { out += ' '; ++col; }
        out.append(s.substr(i, wlen));
        col += wlen;
        while (j < s.size() && s[j] == ' ') ++j;
        i = j;
    }
    return out;
}

inline std::string wrap_text(std::string s, size_t width) {
    return wrap_text(std::string_view(s), width);
}

inline std::string human_bytes(uint64_t b) {
    static const char* kUnits[] = {"B","KiB","MiB","GiB","TiB","PiB"};
    int u = 0;
    auto v = static_cast<double>(b);
    while (v >= 1024.0 && u < 5) { v /= 1024.0; ++u; }
    // show 0 decimals for B/KiB, 1 for others
    if (u <= 1) return fmt::format("{} {}", static_cast<uint64_t>(u==0 ? b : static_cast<uint64_t>(v)), kUnits[u]);
    return fmt::format("{:.1f} {}", v, kUnits[u]);
}

inline std::string ellipsize_middle(std::string s, size_t maxw) {
    if (s.size() <= maxw || maxw < 5) return s;
    const size_t keep = (maxw - 3) / 2;
    const size_t tail = maxw - 3 - keep;
    return s.substr(0, keep) + "..." + s.substr(s.size() - tail);
}

}

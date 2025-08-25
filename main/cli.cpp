#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <arpa/inet.h>
#include <cstring>
#include <fmt/core.h>

static bool readn(const int fd, void* b, size_t n) {
    auto* p = (uint8_t*)b;
    while (n) {
        const ssize_t r = ::read(fd, p, n);
        if (r <= 0) return false;
        p += r;
        n -= r;
    }
    return true;
}

static bool writen(const int fd, const void* b, size_t n) {
    const auto* p = (const uint8_t*)b;
    while (n) {
        const ssize_t w = ::write(fd, p, n);
        if (w <= 0) return false;
        p += w;
        n -= w;
    }
    return true;
}

static std::string ensureNewLine(const std::string& s) {
    if (s.empty() || s.back() != '\n') return s + '\n';
    return s;
}

static bool needs_quotes(std::string_view s) {
    if (s.empty()) return true;
    return std::ranges::any_of(s, [](const auto c) { return c == ' ' || c == '\t' || c == '"' || c == '\\'; });
}

static std::string dq_quote(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (const char c : s) {
        if (c == '"' || c == '\\') out.push_back('\\');
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

// Heuristic: if tail has obvious "value" chars, treat "-Xtail" as glued value
static bool looks_glued_value(std::string_view tail) {
    if (tail.empty()) return false;
    return std::ranges::any_of(tail, [](const char c) {
        return c == '/' || c == '.' || c == ':' || c == '=';
    });
}

// Normalize argv: split --key=value and -Xvalue (heuristic), keep -abc bundles as-is
static std::vector<std::string> normalize_args(const int argc, char** argv, const int start_index) {
    std::vector<std::string> out;
    out.reserve(static_cast<size_t>(argc) - start_index + 4);
    for (int i = start_index; i < argc; ++i) {
        std::string a = argv[i];

        if (a == "--") { out.emplace_back("--"); continue; }

        if (a.rfind("--", 0) == 0) {
            const auto eq = a.find('=');
            if (eq != std::string::npos) {
                out.emplace_back(a.substr(0, eq));          // --key
                out.emplace_back(a.substr(eq + 1));         // value
            } else {
                out.emplace_back(std::move(a));
            }
            continue;
        }

        if (a.size() > 2 && a[0] == '-' && a[1] != '-') {
            // Could be -abc bundle OR -Xvalue
            std::string tail = a.substr(2);
            if (!tail.empty() && looks_glued_value(tail)) {
                out.emplace_back(std::string("-") + a[1]);  // -X
                if (tail[0] == '=') tail.erase(tail.begin());
                out.emplace_back(std::move(tail));          // value
            } else {
                out.emplace_back(std::move(a));             // keep bundle as-is
            }
            continue;
        }

        out.emplace_back(std::move(a)); // plain arg
    }
    return out;
}

static std::string build_line_from_tokens(const std::vector<std::string>& tokens) {
    std::string line;
    for (const auto& t : tokens) {
        if (!line.empty()) line.push_back(' ');
        line += needs_quotes(t) ? dq_quote(t) : t;
    }
    return line;
}

/* ----------------------------------------------------------------- */

int main(const int argc, char** argv) {
    auto cstr_or = [](const char* p, const char* fallback) -> const char* {
        return p ? p : fallback;
    };

    // Decide the "command" token up front (safe even if argv[1] is nullptr)
    const std::string cmd = (argc >= 2 && argv[1] && argv[1][0] != '\0')
                            ? std::string(argv[1])
                            : std::string("help");

    // Build normalized argv: [cmd, args...], with --key=value and -Xvalue split
    std::vector<std::string> argv_norm;
    argv_norm.emplace_back(cmd);

    if (argc >= 3) {
        auto tail = normalize_args(argc, argv, /*start_index=*/2);
        argv_norm.insert(argv_norm.end(), tail.begin(), tail.end());
    }

    // Quoted line for legacy servers (e.g., "group create \"Mile High Club\"")
    std::string line = build_line_from_tokens(argv_norm);

    const int s = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (s < 0) { perror("socket"); return 1; }

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "/run/vaulthalla/cli.sock");
    if (::connect(s, (sockaddr*)&addr, sizeof(sa_family_t) + std::strlen(addr.sun_path) + 1) != 0) {
        perror("connect");
        ::close(s);
        return 1;
    }

    // Back-compat fields + richer payload
    nlohmann::json j;
    j["cmd"]  = argv_norm.front();
    if (argv_norm.size() > 1) {
        std::vector<std::string> args(argv_norm.begin() + 1, argv_norm.end());
        j["args"] = args;                    // old shape
    } else {
        j["args"] = nlohmann::json::array();
    }
    j["argv"] = argv_norm;                   // full normalized argv (cmd-first)
    j["line"] = line;                        // canonical quoted line (for string parsers)

    const auto req = j.dump();
    uint32_t n = htonl(static_cast<uint32_t>(req.size()));
    if (!writen(s, &n, 4) || !writen(s, req.data(), req.size())) {
        ::close(s);
        return 1;
    }

    uint32_t len_be;
    if (!readn(s, &len_be, 4)) { ::close(s); return 1; }
    const uint32_t len = ntohl(len_be);
    std::string resp(len, '\0');
    if (!readn(s, resp.data(), len)) { ::close(s); return 1; }
    ::close(s);

    auto r = nlohmann::json::parse(resp, nullptr, /*allow_exceptions=*/false);
    if (r.is_discarded()) {
        fmt::print(stderr, "{}", "Invalid JSON response\n");
        return 1;
    }

    if (r.contains("stdout")) fmt::print("{}", ensureNewLine(r["stdout"].get<std::string>()));
    if (r.contains("stderr")) fmt::print(stderr, "{}", ensureNewLine(r["stderr"].get<std::string>()));
    return r.value("exit_code", 0);
}

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstdint>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <iostream>
#include <arpa/inet.h>
#include <cstring>
#include <fmt/core.h>

static bool readn(int fd, void* b, size_t n) {
    auto* p = (uint8_t*)b;
    while (n) {
        ssize_t r = ::read(fd, p, n);
        if (r <= 0) return false;
        p += r;
        n -= r;
    }
    return true;
}

static bool writen(int fd, const void* b, size_t n) {
    auto* p = (const uint8_t*)b;
    while (n) {
        ssize_t w = ::write(fd, p, n);
        if (w <= 0) return false;
        p += w;
        n -= w;
    }
    return true;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: vhctl-uds <cmd> [args...]\n";
        return 2;
    }
    int s = ::socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "/run/vaulthalla/cli.sock");
    if (::connect(s, (sockaddr*)&addr, sizeof(sa_family_t) + std::strlen(addr.sun_path) + 1) != 0) {
        perror("connect");
        return 1;
    }

    nlohmann::json j;
    j["cmd"] = argv[1];
    std::vector<std::string> args;
    for (int i = 2; i < argc; ++i) args.emplace_back(argv[i]);
    j["args"] = args;
    auto req = j.dump();
    uint32_t n = htonl(static_cast<uint32_t>(req.size()));
    writen(s, &n, 4);
    writen(s, req.data(), req.size());

    uint32_t len_be;
    if (!readn(s, &len_be, 4)) return 1;
    uint32_t len = ntohl(len_be);
    std::string resp(len, '\0');
    if (!readn(s, resp.data(), len)) return 1;

    auto r = nlohmann::json::parse(resp);
    if (r.contains("stdout")) fmt::print("{}",
    r["stdout"].get<std::string>());
    if (r.contains("stderr")) fmt::print(stderr, "{}",
        r["stderr"].get<std::string>());
    return r.value("exit_code", 0);
}
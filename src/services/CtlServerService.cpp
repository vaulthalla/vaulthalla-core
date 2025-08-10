#include "services/CtlServerService.hpp"
#include "protocols/shell/Router.hpp"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <cstring>
#include <stdexcept>
#include <grp.h>
#include <pwd.h>

using nlohmann::json;

namespace {

struct Peer { uid_t uid; gid_t gid; pid_t pid; };

Peer peercred(int fd) {
    struct ucred c{}; socklen_t len = sizeof(c);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &c, &len) != 0)
        throw std::runtime_error("SO_PEERCRED failed");
    return {c.uid, c.gid, c.pid};
}

bool in_group(uid_t uid, gid_t admin_gid) {
    if (uid == 0) return true;
    // Quick check: primary group ok?
    struct passwd* pw = getpwuid(uid);
    if (!pw) return false;
    if (pw->pw_gid == admin_gid) return true;
    int ng = 0;
    getgrouplist(pw->pw_name, pw->pw_gid, nullptr, &ng);
    std::vector<gid_t> gs(ng);
    if (getgrouplist(pw->pw_name, pw->pw_gid, gs.data(), &ng) < 0) return false;
    for (auto g : gs) if (g == admin_gid) return true;
    return false;
}

bool readn(int fd, void* buf, size_t n) {
    auto* p = static_cast<unsigned char*>(buf);
    while (n) { ssize_t r = ::read(fd, p, n); if (r <= 0) return false; p += r; n -= r; }
    return true;
}
bool writen(int fd, const void* buf, size_t n) {
    auto* p = static_cast<const unsigned char*>(buf);
    while (n) { ssize_t w = ::write(fd, p, n); if (w <= 0) return false; p += w; n -= w; }
    return true;
}

void send_json(int fd, const json& j) {
    auto s = j.dump();
    uint32_t len = htonl(static_cast<uint32_t>(s.size()));
    (void)writen(fd, &len, 4);
    (void)writen(fd, s.data(), s.size());
}

} // namespace

namespace vh::services {

CtlServerService::CtlServerService()
  : AsyncService("vaulthalla-cli"),
    router_(std::make_shared<shell::Router>()),
    socketPath_("/run/vaulthalla/cli.sock"),
    adminGid_(getgrnam("vaulthalla")->gr_gid) {}

CtlServerService::~CtlServerService() { closeListener(); }

void CtlServerService::closeListener() {
    if (listenFd_ >= 0) { ::close(listenFd_); listenFd_ = -1; }
    ::unlink(socketPath_.c_str());
}

void CtlServerService::onStop() {
    // Called by AsyncService::stop() before join; close to break accept()
    closeListener();
}

void CtlServerService::runLoop() {
    // Bind
    ::unlink(socketPath_.c_str());
    listenFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenFd_ < 0) throw std::runtime_error("socket()");
    sockaddr_un addr{}; addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socketPath_.c_str());
    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr),
               sizeof(sa_family_t) + std::strlen(addr.sun_path) + 1) != 0)
        throw std::runtime_error("bind()");
    ::chmod(socketPath_.c_str(), 0660);
    // chown to vaulthalla:vaulthalla-admin in your service startup if needed

    if (::listen(listenFd_, 16) != 0) throw std::runtime_error("listen()");

    // Accept loop
    while (running_) {
        int cfd = ::accept4(listenFd_, nullptr, nullptr, SOCK_CLOEXEC);
        if (cfd < 0) {
            if (!running_) break; // listener closed during stop
            continue;
        }

        // Handle one request/response per connection (simple MVP)
        try {
            auto p = peercred(cfd);
            if (!in_group(p.uid, static_cast<gid_t>(adminGid_))) {
                send_json(cfd, {{"ok", false}, {"exit_code", 1}, {"message", "permission denied"}});
                ::close(cfd);
                continue;
            }

            uint32_t be = 0;
            if (!readn(cfd, &be, 4)) { ::close(cfd); continue; }
            uint32_t len = ntohl(be);
            if (len > (1u<<20)) { // 1 MiB cap
                send_json(cfd, {{"ok",false},{"exit_code",1},{"message","request too large"}});
                ::close(cfd); continue;
            }
            std::string body(len, '\0');
            if (!readn(cfd, body.data(), len)) { ::close(cfd); continue; }

            json req = json::parse(body, nullptr, true);
            std::string cmd = req.value("cmd", "");
            std::vector<std::string> args = req.value("args", std::vector<std::string>{});
            if (cmd.empty()) cmd = "help";

            int exit_code = 0;
            try {
                router_->execute(cmd, args);
            } catch (const std::invalid_argument& e) {
                exit_code = 1;
                send_json(cfd, {{"ok",false},{"exit_code",exit_code},{"message",e.what()}});
                ::close(cfd); continue;
            } catch (const std::exception& e) {
                exit_code = 1;
                send_json(cfd, {{"ok",false},{"exit_code",exit_code},{"message",e.what()}});
                ::close(cfd); continue;
            }

            send_json(cfd, {{"ok",true},{"exit_code",0},{"message","ok"}});
        } catch (...) {
            // best-effort error response
            send_json(cfd, {{"ok",false},{"exit_code",1},{"message","internal error"}});
        }
        ::close(cfd);
    }
}

} // namespace vh::services

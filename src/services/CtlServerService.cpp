#include "services/CtlServerService.hpp"
#include "protocols/shell/Router.hpp"
#include "protocols/shell/Parser.hpp"
#include "protocols/shell/commands.hpp"
#include "database/Queries/UserQueries.hpp"
#include "services/LogRegistry.hpp"
#include "protocols/shell/SocketIO.hpp"
#include "types/User.hpp"

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

using namespace vh::database;
using namespace vh::logging;

namespace {

struct Peer {
    uid_t uid;
    gid_t gid;
    pid_t pid;
};

Peer peercred(const int fd) {
    ucred c{};
    socklen_t len = sizeof(c);
    if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &c, &len) != 0) throw std::runtime_error("SO_PEERCRED failed");
    return {c.uid, c.gid, c.pid};
}

bool in_group(uid_t uid, gid_t admin_gid) {
    if (uid == 0) return true;
    // Quick check: primary group ok?
    const passwd* pw = getpwuid(uid);
    if (!pw) return false;
    if (pw->pw_gid == admin_gid) return true;
    int ng = 0;
    getgrouplist(pw->pw_name, pw->pw_gid, nullptr, &ng);
    std::vector<gid_t> gs(ng);
    if (getgrouplist(pw->pw_name, pw->pw_gid, gs.data(), &ng) < 0) return false;
    return std::ranges::any_of(gs, [admin_gid](gid_t g) { return g == admin_gid; });
}

bool readn(const int fd, void* buf, size_t n) {
    auto* p = static_cast<unsigned char*>(buf);
    while (n) {
        const ssize_t r = ::read(fd, p, n);
        if (r <= 0) return false;
        p += r;
        n -= r;
    }
    return true;
}

bool writen(const int fd, const void* buf, size_t n) {
    auto* p = static_cast<const unsigned char*>(buf);
    while (n) {
        const ssize_t w = ::write(fd, p, n);
        if (w <= 0) return false;
        p += w;
        n -= w;
    }
    return true;
}

void send_json(const int fd, const json& j) {
    const auto s = j.dump();
    const auto len = htonl(static_cast<uint32_t>(s.size()));
    (void)writen(fd, &len, 4);
    (void)writen(fd, s.data(), s.size());
}

} // namespace

using namespace vh::shell;

namespace vh::services {

CtlServerService::CtlServerService()
    : AsyncService("vaulthalla-cli"),
      router_(std::make_shared<shell::Router>()),
      socketPath_("/run/vaulthalla/cli.sock"),
      adminGid_(getgrnam("vaulthalla")->gr_gid),
      adminUIDSet_(false) {
    shell::registerAllCommands(router_);

    const auto admin = UserQueries::getUserByName("admin");
    if (!admin) LogRegistry::shell()->warn("[CtlServerService] No 'admin' user found in database");
    if (admin->linux_uid.has_value()) adminUIDSet_.store(true);
}

CtlServerService::~CtlServerService() { closeListener(); }

void CtlServerService::closeListener() {
    if (listenFd_ >= 0) {
        ::close(listenFd_);
        listenFd_ = -1;
    }
    ::unlink(socketPath_.c_str());
}

void CtlServerService::onStop() {
    // Called by AsyncService::stop() before join; close to break accept()
    closeListener();
}

void CtlServerService::initAdminUid(const int cfd, const uid_t uid) {
    const auto msg = fmt::format(
        "[CtlServerService] 'admin' user has no Linux UID set; Assigning first user UID {} to 'admin'", uid);
    LogRegistry::shell()->info(msg);
    LogRegistry::audit()->info(msg);

    const struct passwd* pw = getpwuid(uid);
    if (!pw) throw std::runtime_error("Unable to resolve UID to username");
    const std::string uname = pw->pw_name;

    LogRegistry::shell()->info("[CtlServerService] Adding UID {} to vaulthalla group", uid);

    // Assign UID to admin
    const auto admin = UserQueries::getUserByName("admin");
    if (!admin) throw std::runtime_error("admin user disappeared");
    admin->linux_uid = uid;
    UserQueries::updateUser(admin);
    adminUIDSet_.store(true);

    LogRegistry::shell()->info("[CtlServerService] Assigned UID {} to 'admin' user", uid);

    // Check if user is already in the vaulthalla group
    struct group* grp = getgrnam("vaulthalla");
    if (!grp) throw std::runtime_error("Group 'vaulthalla' not found");

    bool in_group = false;
    for (char** mem = grp->gr_mem; *mem != nullptr; ++mem) {
        if (uname == *mem) {
            in_group = true;
            break;
        }
    }

    if (!in_group) {
        // Fall back to system() since group modification isn't possible without `usermod`
        std::string cmd = fmt::format(kAddAdminCmd, uname);
        LogRegistry::shell()->debug("[CtlServerService] Running fallback group add command: {}", cmd);

        if (const int result = std::system(cmd.c_str()); result != 0) {
            LogRegistry::shell()->warn("[CtlServerService] usermod failed, checking group manually...");

            // Retry group check (maybe the change propagated anyway)
            endgrent(); // flush group cache
            grp = getgrnam("vaulthalla");
            if (!grp) throw std::runtime_error("Group 'vaulthalla' not found after fallback");

            in_group = false;
            for (char** mem = grp->gr_mem; *mem != nullptr; ++mem) {
                if (uname == *mem) {
                    in_group = true;
                    break;
                }
            }

            if (!in_group) {
                LogRegistry::shell()->error("[CtlServerService] Failed to add '{}' to vaulthalla group", uname);
                send_json(cfd, {{"ok", false}, {"exit_code", 1}, {"stderr", "failed to add to vaulthalla group"}});
                ::close(cfd);
                throw std::runtime_error("Group add failed and could not verify fallback");
            }
        }
    }

    LogRegistry::shell()->info("[CtlServerService] Verified '{}' in vaulthalla group", uname);
    LogRegistry::audit()->info("Promoted user '{}' (UID {}) to admin group", uname, uid);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

void CtlServerService::runLoop() {
    // Bind
    ::unlink(socketPath_.c_str());
    listenFd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (listenFd_ < 0) throw std::runtime_error("socket()");
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::snprintf(addr.sun_path, sizeof(addr.sun_path), "%s", socketPath_.c_str());
    if (::bind(listenFd_, reinterpret_cast<sockaddr*>(&addr),
               sizeof(sa_family_t) + std::strlen(addr.sun_path) + 1) != 0)
        throw std::runtime_error("bind()");
    ::chmod(socketPath_.c_str(), 0660);
    // chown to vaulthalla:vaulthalla in your service startup if needed

    if (::listen(listenFd_, 16) != 0) throw std::runtime_error("listen()");

    // Accept loop
    while (running_) {
        int cfd = ::accept4(listenFd_, nullptr, nullptr, SOCK_CLOEXEC);
        if (cfd < 0) {
            if (!running_) break; // listener closed during stop
            continue;
        }

        try {
            const auto p = peercred(cfd);

            LogRegistry::shell()->warn("[CtlServerService] Connection from UID {} (PID {})", p.uid, p.pid);

            if (!adminUIDSet_.load() && p.uid != 0 && p.uid >= 1000) initAdminUid(cfd, p.uid);

            if (!in_group(p.uid, adminGid_)) {
                LogRegistry::shell()->warn("[CtlServerService] Connection from UID {} (PID {}) not in vaulthalla group",
                                           p.uid, p.pid);
                send_json(cfd, {{"ok", false}, {"exit_code", 1}, {"stderr", "permission denied"}});
                ::close(cfd);
                continue;
            }

            const auto user = UserQueries::getUserByLinuxUID(p.uid);
            if (!user) {
                LogRegistry::shell()->warn("[CtlServerService] No user found for UID {} (PID {})", p.uid, p.pid);
                send_json(cfd, {{"ok", false}, {"exit_code", 1}, {"stderr", "user not found"}});
                ::close(cfd);
                continue;
            }

            uint32_t be = 0;
            if (!readn(cfd, &be, 4)) {
                ::close(cfd);
                continue;
            }
            const uint32_t len = ntohl(be);
            if (len > (1u << 20)) {
                // 1 MiB cap
                send_json(cfd, {{"ok", false}, {"exit_code", 1}, {"stderr", "request too large"}});
                ::close(cfd);
                continue;
            }
            std::string body(len, '\0');
            if (!readn(cfd, body.data(), len)) {
                ::close(cfd);
                continue;
            }

            json req = json::parse(body, nullptr, true);
            std::string cmd = req.value("cmd", "");
            std::vector<std::string> args = req.value("args", std::vector<std::string>{});
            if (cmd.empty()) cmd = "help";

            // ... after parsing `req` ...
            int exit_code = 0;

            auto try_exec_line = [&](const std::string& line) {
                try {
                    const auto res = router_->executeLine(line, user); // uses tokenize() + parseTokens() under the hood

                    json resp{
                        {"ok", res.exit_code == 0},
                        {"exit_code", res.exit_code},
                    };
                    if (!res.stdout_text.empty()) resp["stdout"] = res.stdout_text;
                    if (!res.stderr_text.empty()) resp["stderr"] = res.stderr_text;
                    if (res.has_data) resp["data"] = res.data;

                    send_json(cfd, resp);

                    send_json(cfd, {{"ok", true}, {"exit_code", 0}, {"stderr", "ok"}});
                } catch (const std::invalid_argument& e) {
                    send_json(cfd, {{"ok", false}, {"exit_code", 1}, {"stderr", e.what()}});
                } catch (const std::exception& e) {
                    send_json(cfd, {{"ok", false}, {"exit_code", 1}, {"stderr", e.what()}});
                }
            };

            if (req.contains("line") && req["line"].is_string()) {
                auto line = req["line"].get<std::string>();
                if (line.empty()) line = "help";

                std::unique_ptr<SocketIO> io;

                if (req.value("interactive", false))
                    io = std::make_unique<SocketIO>(cfd);

                try {
                    const auto res = router_->executeLine(line, user, io.get());

                    json reply{
                {"type", "result"},
                {"ok", res.exit_code == 0},
                {"exit_code", res.exit_code}
                    };
                    if (!res.stdout_text.empty()) reply["stdout"] = res.stdout_text;
                    if (!res.stderr_text.empty()) reply["stderr"] = res.stderr_text;
                    if (res.has_data) reply["data"] = res.data;

                    SocketIO::send_json(cfd, reply);
                } catch (const std::exception& e) {
                    SocketIO::send_json(cfd, {
                        {"type", "result"},
                        {"ok", false},
                        {"exit_code", 1},
                        {"stderr", e.what()}
                    });
                }

                ::close(cfd);
                continue;
            }

            LogRegistry::shell()->error("[CtlServerService] No 'line' field in request from UID {} (PID {})", p.uid,
                                        p.pid);
            send_json(cfd, {{"ok", false}, {"exit_code", 1}, {"stderr", "invalid request"}});
        } catch (...) {
            // best-effort error response
            send_json(cfd, {{"ok", false}, {"exit_code", 1}, {"stderr", "internal error"}});
        }
        ::close(cfd);
    }
}

}

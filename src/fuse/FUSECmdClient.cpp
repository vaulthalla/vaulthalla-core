#include "fuse/FUSECmdClient.hpp"
#include "types/fuse/Command.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace vh::fuse::ipc {

bool sendCommand(const std::string& socketPath, const types::fuse::Command& cmd) {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return false;
    }

    sockaddr_un addr{};
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socketPath.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sock);
        return false;
    }

    nlohmann::json jsonCmd = {{"op", types::fuse::to_string(cmd.type)},
                              {"path", cmd.path},
                              {"uid", cmd.uid},
                              {"gid", cmd.gid},
                              {"mode", cmd.mode}};

    std::string payload = jsonCmd.dump();
    if (write(sock, payload.c_str(), payload.size()) == -1) {
        perror("write");
        close(sock);
        return false;
    }

    close(sock);
    return true;
}

} // namespace vh::fuse::ipc

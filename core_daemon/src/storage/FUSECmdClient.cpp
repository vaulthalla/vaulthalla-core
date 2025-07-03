#include "storage/FUSECmdClient.hpp"
#include "fuse/Command.hpp"
#include "config/ConfigRegistry.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace vh::storage {

bool sendCommand(const types::fuse::Command& cmd) {
    const int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return false;
    }

    sockaddr_un addr{};
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, config::ConfigRegistry::get().server.uds_socket.c_str(), sizeof(addr.sun_path) - 1);

    if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == -1) {
        perror("connect");
        close(sock);
        return false;
    }

    const nlohmann::json jsonCmd = {{"op", types::fuse::to_string(cmd.type)},
                              {"path", cmd.path},
                              {"uid", cmd.uid},
                              {"gid", cmd.gid},
                              {"mode", cmd.mode}};

    const std::string payload = jsonCmd.dump();
    if (write(sock, payload.c_str(), payload.size()) == -1) {
        perror("write");
        close(sock);
        return false;
    }

    close(sock);
    return true;
}

} // namespace vh::storage

#include "protocols/FUSECmdClient.hpp"
#include "types/FUSECommand.hpp"
#include "config/ConfigRegistry.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace vh::storage {

bool sendCommand(const types::fuse::FUSECommand& cmd) {
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

    nlohmann::json jsonCmd = {{"op", types::fuse::to_string(cmd.type)},
                                    {"vaultId", cmd.vaultId}};

    if (cmd.fsEntryId) jsonCmd["fsEntryId"] = cmd.fsEntryId;
    if (cmd.from) jsonCmd["from"] = *cmd.from;
    if (cmd.to) jsonCmd["to"] = *cmd.to;

    const std::string payload = jsonCmd.dump();
    if (write(sock, payload.c_str(), payload.size()) == -1) {
        perror("write");
        close(sock);
        return false;
    }

    close(sock);
    return true;
}

void sendSyncCommand(const unsigned int vaultId) {
    types::fuse::FUSECommand cmd;
    cmd.type = types::fuse::CommandType::SYNC;
    cmd.vaultId = vaultId;

    if (!sendCommand(cmd)) std::cerr << "[-] Failed to send sync command for vault " << vaultId << std::endl;
}

void sendRegisterCommand(const unsigned int vaultId, const unsigned int fsEntryId) {
    types::fuse::FUSECommand cmd;
    cmd.type = types::fuse::CommandType::REGISTER;
    cmd.vaultId = vaultId;
    cmd.fsEntryId = std::make_optional(fsEntryId);

    if (!sendCommand(cmd)) std::cerr << "[-] Failed to send register command for entry " << fsEntryId << std::endl;
}

void sendRenameCommand(const unsigned int vaultId, const std::filesystem::path& from, const std::filesystem::path& to) {
    types::fuse::FUSECommand cmd;
    cmd.type = types::fuse::CommandType::RENAME;
    cmd.vaultId = vaultId;
    cmd.from = from;
    cmd.to = to;

    if (!sendCommand(cmd)) std::cerr << "[-] Failed to send rename command from " << from << " to " << to << std::endl;
}

} // namespace vh::storage

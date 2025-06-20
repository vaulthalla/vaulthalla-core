#pragma once

#include <string>

namespace vh::types::fuse {
    struct Command;
}

namespace vh::fuse::ipc {

    // Sends a Command struct over UDS to the FUSE daemon
    bool sendCommand(const std::string& socketPath, const types::fuse::Command& cmd);

}

#pragma once

namespace vh::types::fuse {
struct Command;
}

namespace vh::storage {

// Sends a Command struct over UDS to the FUSE daemon
bool sendCommand(const types::fuse::Command& cmd);

} // namespace vh::storage

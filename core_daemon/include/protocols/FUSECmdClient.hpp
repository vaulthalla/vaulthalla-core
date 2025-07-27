#pragma once

namespace vh::types::fuse {
struct FUSECommand;
}

namespace vh::storage {

bool sendCommand(const types::fuse::FUSECommand& cmd);

void sendSyncCommand(unsigned int vaultId);

void sendRegisterCommand(unsigned int vaultId, unsigned int fsEntryId);

} // namespace vh::storage

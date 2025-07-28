#pragma once

#include <filesystem>

namespace vh::types::fuse {
struct FUSECommand;
}

namespace vh::storage {

bool sendCommand(const types::fuse::FUSECommand& cmd);

void sendSyncCommand(unsigned int vaultId);

void sendRegisterCommand(unsigned int vaultId, unsigned int fsEntryId);

void sendRenameCommand(unsigned int vaultId, const std::filesystem::path& from, const std::filesystem::path& to);

} // namespace vh::storage

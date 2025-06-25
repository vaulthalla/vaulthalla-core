#pragma once

#include <vector>
#include <cstdint>

namespace vh::types {

enum class FileACL : uint16_t {
    Read            = 1 << 0,
    Write           = 1 << 1,
    Delete          = 1 << 2,
    ManageMetadata  = 1 << 3,
    Lock            = 1 << 4,
    Share           = 1 << 5,
    Execute         = 1 << 6, // For directory traversal or executable files
    Rename          = 1 << 7,
    Move            = 1 << 8,
    ChangeOwner     = 1 << 9
};

uint16_t toBitmask(const std::vector<FileACL>& perms);
std::vector<FileACL> aclFromBitmask(uint16_t mask);
bool hasPermission(uint16_t mask, FileACL permission);

} // namespace vh::types

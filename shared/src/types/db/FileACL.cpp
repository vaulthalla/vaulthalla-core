#include "types/db/FileACL.hpp"

namespace vh::types {

uint16_t toBitmask(const std::vector<FileACL>& perms) {
    uint16_t mask = 0;
    for (auto p : perms) mask |= static_cast<uint16_t>(p);
    return mask;
}

std::vector<FileACL> aclFromBitmask(const uint16_t mask) {
    std::vector<FileACL> perms;
    for (int bit = 0; bit < 16; ++bit) {
        auto value = static_cast<FileACL>(1 << bit);
        if (mask & static_cast<uint16_t>(value)) {
            perms.push_back(value);
        }
    }
    return perms;
}

bool hasPermission(const uint16_t mask, FileACL permission) {
    return (mask & static_cast<uint16_t>(permission)) != 0;
}

} // namespace vh::types

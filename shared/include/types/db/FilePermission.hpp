#pragma once

#include <boost/describe.hpp>
#include <vector>

namespace vh::types {

enum class FilePermission : uint32_t {
    Read = 1 << 0,
    Write = 1 << 1,
    Delete = 1 << 2,
    ManageMetadata = 1 << 3,
    Lock = 1 << 4,
    Share = 1 << 5,
    // You can add more here later — no schema change required!
};

inline uint32_t toBitmask(const std::vector<FilePermission>& perms) {
    uint32_t mask = 0;
    for (auto p : perms) {
        mask |= static_cast<uint32_t>(p);
    }
    return mask;
}

inline std::vector<FilePermission> fromBitmask(uint32_t mask) {
    std::vector<FilePermission> perms;
    if (mask & static_cast<uint32_t>(FilePermission::Read)) perms.push_back(FilePermission::Read);
    if (mask & static_cast<uint32_t>(FilePermission::Write)) perms.push_back(FilePermission::Write);
    if (mask & static_cast<uint32_t>(FilePermission::Delete)) perms.push_back(FilePermission::Delete);
    if (mask & static_cast<uint32_t>(FilePermission::ManageMetadata)) perms.push_back(FilePermission::ManageMetadata);
    if (mask & static_cast<uint32_t>(FilePermission::Lock)) perms.push_back(FilePermission::Lock);
    if (mask & static_cast<uint32_t>(FilePermission::Share)) perms.push_back(FilePermission::Share);
    return perms;
}
} // namespace vh::types

BOOST_DESCRIBE_ENUM(vh::types::FilePermission, Read, Write, Delete, ManageMetadata, Lock, Share)

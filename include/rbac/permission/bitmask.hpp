#pragma once

#include <cstdint>
#include <vector>
#include <bitset>
#include <string>

namespace vh::rbac::permission {

template <typename T>
uint16_t toBitmask(const std::vector<T>& perms) {
    uint16_t mask = 0;
    for (const auto perm : perms) mask |= static_cast<uint16_t>(perm);
    return mask;
}

template <typename T>
std::vector<T> permsFromBitmask(const uint16_t mask) {
    std::vector<T> result;
    for (uint16_t bit = 0; bit < 16; ++bit) {
        const uint16_t val = static_cast<uint16_t>(1u << bit);
        if ((mask & val) != 0) result.push_back(static_cast<T>(val));
    }
    return result;
}

template <typename T>
bool hasPermission(const uint16_t mask, const T perm) {
    return (mask & static_cast<uint16_t>(perm)) != 0;
}

std::string bitStringFromMask(uint16_t mask);
std::bitset<16> toBitset(uint16_t mask);

}

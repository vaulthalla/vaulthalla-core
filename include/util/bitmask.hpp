#pragma once

#include <cstdint>
#include <string>
#include <bitset>

namespace vh::util::bitmask {
std::string permissions_to_bitstring(uint16_t perms);
std::bitset<16> bitmask_to_bitset(uint16_t mask);
}

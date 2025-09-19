#pragma once

#include <cstdint>
#include <string>
#include <bitset>

namespace vh::util::bitmask {
std::string bitStringFromMask(uint16_t mask);
std::bitset<16> bitmask_to_bitset(uint16_t mask);
}

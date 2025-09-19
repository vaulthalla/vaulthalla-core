#include "util/bitmask.hpp"

using namespace vh::util::bitmask;

std::string vh::util::bitmask::bitStringFromMask(const uint16_t mask) {
    std::string out = "B";
    for (int i = 15; i >= 0; --i) out += (mask & (1 << i)) ? '1' : '0';
    out += "";
    return out;
}

std::bitset<16> vh::util::bitmask::bitmask_to_bitset(const uint16_t mask) {
    return {mask};
}

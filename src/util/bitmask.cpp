#include "util/bitmask.hpp"

using namespace vh::util::bitmask;

std::string vh::util::bitmask::permissions_to_bitstring(const uint16_t perms) {
    std::string bitstring = "B'";
    for (int i = 15; i >= 0; --i) bitstring += ((perms >> i) & 1) ? '1' : '0';
    bitstring += "'";
    return bitstring;
}

std::bitset<16> vh::util::bitmask::bitmask_to_bitset(const uint16_t mask) {
    return {mask};
}

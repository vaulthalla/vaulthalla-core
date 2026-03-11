#include "rbac/permission/bitmask.hpp"

namespace vh::rbac::permission {

std::string bitStringFromMask(const uint16_t mask) {
    std::string out = "B";
    for (int i = 15; i >= 0; --i) out += (mask & (1 << i)) ? '1' : '0';
    out += "";
    return out;
}

std::bitset<16> toBitset(const uint16_t mask) {
    return {mask};
}

}

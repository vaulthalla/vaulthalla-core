#pragma once

#include <cstdint>
#include <string>

namespace vh::util::bitmask {
std::string permissions_to_bitstring(uint16_t perms);
}

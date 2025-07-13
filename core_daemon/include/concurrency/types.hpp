#pragma once

#include <variant>
#include <vector>
#include <cstdint>

typedef std::variant<std::vector<uint8_t>, bool, void> ExpectedFuture;

#pragma once

#include <memory>
#include <variant>

namespace vh::types {
struct VaultStat;
}

typedef std::variant<bool, std::shared_ptr<vh::types::VaultStat>> ExpectedFuture;

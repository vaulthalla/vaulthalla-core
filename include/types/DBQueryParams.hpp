#pragma once

#include <string>
#include <optional>

namespace vh::types {

enum class SortOrder { ASC, DESC };

inline std::optional<std::string> to_string(const std::optional<SortOrder>& order) {
    if (!order) return std::nullopt;
    return order == SortOrder::ASC ? "asc" : "desc";
}

struct DBQueryParams {
    std::optional<std::string> sortBy{};
    std::optional<SortOrder> order{};
    std::optional<unsigned int> limit{}, offset{};
};

}

#pragma once

#include <filesystem>

inline std::filesystem::path common_path_prefix(const std::filesystem::path& a, const std::filesystem::path& b) {
    std::filesystem::path result;
    auto ait = a.begin();
    auto bit = b.begin();

    while (ait != a.end() && bit != b.end() && *ait == *bit) {
        result /= *ait;
        ++ait;
        ++bit;
    }

    return result;
}

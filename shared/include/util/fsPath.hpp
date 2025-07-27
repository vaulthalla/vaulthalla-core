#pragma once

#include <filesystem>

namespace fs = std::filesystem;

inline fs::path common_path_prefix(const fs::path& a, const fs::path& b) {
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

inline fs::path makeAbsolute(const fs::path& absPath) {
    if (absPath.empty() || absPath.string().front() == '/') return absPath;
    return fs::path("/") / absPath;
}

inline fs::path resolveParent(const fs::path& absPath) {
    if (absPath.empty()) return {"/"};
    return absPath.has_parent_path() ? absPath.parent_path() : fs::path("/");
}

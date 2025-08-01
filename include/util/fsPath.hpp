#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <ranges>

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

inline fs::path makeAbsolute(const fs::path& path) {
    if (path.empty()) return "/";
    if (path.is_absolute()) return path.lexically_normal();
    return fs::path("/") / path.lexically_normal();
}

inline fs::path resolveParent(const fs::path& path) {
    if (path.empty()) return "/";
    auto norm = path.lexically_normal();
    if (norm == "/") return "/";
    return norm.parent_path().empty() ? "/" : norm.parent_path();
}

inline fs::path stripLeadingSlash(const fs::path& path) {
    if (path.empty()) return "/";
    auto norm = path.lexically_normal();
    if (norm.empty() || norm == "/") return "/";
    if (norm.string().front() == '/') return { norm.string().substr(1) };
    return norm;
}

inline std::string inferMimeTypeFromPath(const fs::path& path) {
    static const std::unordered_map<std::string, std::string> mimeMap = {
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".png", "image/png"},
        {".pdf", "application/pdf"}, {".txt", "text/plain"}, {".html", "text/html"},
    };

    std::string ext = path.extension().string();
    std::ranges::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    const auto it = mimeMap.find(ext);
    return it != mimeMap.end() ? it->second : "application/octet-stream";
}

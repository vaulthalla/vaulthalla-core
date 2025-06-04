#pragma once
#include <string>
#include <chrono>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace vh::core {

    struct FileMetadata {
        std::string path;
        uintmax_t size_bytes = 0;
        std::filesystem::perms permissions;
        std::string owner;
        std::string group;

        std::chrono::system_clock::time_point created_at;
        std::chrono::system_clock::time_point modified_at;
        std::chrono::system_clock::time_point accessed_at;

        FileMetadata() = default;
        explicit FileMetadata(const std::filesystem::directory_entry& entry);

        [[nodiscard]] nlohmann::json to_json() const;
        static FileMetadata from_json(const nlohmann::json& j);
    };

} // namespace vh::core

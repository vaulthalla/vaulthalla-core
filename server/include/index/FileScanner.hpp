#pragma once

#include <filesystem>
#include <string>
#include <optional>
#include <chrono>

namespace vh::index {

    struct FileMetadata {
        std::filesystem::path path;
        uintmax_t size = 0;
        std::chrono::file_clock::time_point lastModified;
        bool isTextFile = false;
        std::optional<std::string> contentPreview; // e.g. first 1KB for now
    };

    class FileScanner {
    public:
        FileMetadata scan(const std::filesystem::path& path) const;

    private:
        bool isText(const std::filesystem::path& path) const;
        std::optional<std::string> readPreview(const std::filesystem::path& path, size_t maxBytes = 1024) const;
    };

} // namespace vh::index

#include "index/FileScanner.hpp"
#include <fstream>
#include <sstream>
#include <iterator>
#include <cctype>

namespace vh::index {

    FileMetadata FileScanner::scan(const std::filesystem::path& path) const {
        FileMetadata metadata;
        metadata.path = path;

        try {
            metadata.size = std::filesystem::file_size(path);
            metadata.lastModified = std::filesystem::last_write_time(path);
            metadata.isTextFile = isText(path);

            if (metadata.isTextFile) {
                metadata.contentPreview = readPreview(path);
            }

        } catch (const std::exception& e) {
            // You can add logging here
        }

        return metadata;
    }

    bool FileScanner::isText(const std::filesystem::path& path) const {
        std::ifstream file(path, std::ios::binary);
        if (!file) return false;

        constexpr size_t checkSize = 512;
        char buffer[checkSize];
        file.read(buffer, checkSize);
        std::streamsize bytesRead = file.gcount();

        for (std::streamsize i = 0; i < bytesRead; ++i) {
            unsigned char c = static_cast<unsigned char>(buffer[i]);
            if (c < 32 && c != 9 && c != 10 && c != 13) {
                return false; // Non-text character found
            }
        }

        return true;
    }

    std::optional<std::string> FileScanner::readPreview(const std::filesystem::path& path, size_t maxBytes) const {
        std::ifstream file(path);
        if (!file) return std::nullopt;

        std::ostringstream ss;
        ss << file.rdbuf();

        std::string content = ss.str();
        if (content.size() > maxBytes) {
            content.resize(maxBytes);
        }

        return content;
    }

} // namespace vh::index

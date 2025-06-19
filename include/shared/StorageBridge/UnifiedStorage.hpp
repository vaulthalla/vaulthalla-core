#pragma once

#include <string>
#include <vector>

namespace vh::types {
    class File;
}

namespace vh::shared::bridge {

    class UnifiedStorage {
    public:
        UnifiedStorage(); // Eventually: pass vault config

        [[nodiscard]] bool exists(const std::string& path) const;
        [[nodiscard]] types::File getMetadata(const std::string& path) const;
        [[nodiscard]] std::vector<types::File> listDirectory(const std::string& path) const;

        std::vector<char> readFile(const std::string& path, size_t offset, size_t size);
        ssize_t writeFile(const std::string& path, const char* buf, size_t size, size_t offset);

        bool makeDirectory(const std::string& path);
        bool removeFile(const std::string& path);
        bool moveFile(const std::string& oldPath, const std::string& newPath);
    };

}

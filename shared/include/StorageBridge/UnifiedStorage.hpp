#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace vh::types {
struct File;
}

namespace vh::shared::bridge {

class UnifiedStorage {
  public:
    UnifiedStorage(); // TODO: add vault config and dependency injection

    // Metadata + Existence
    [[nodiscard]] bool exists(const std::string& path) const;
    [[nodiscard]] types::File getMetadata(const std::string& path) const;
    [[nodiscard]] std::vector<types::File> listDirectory(const std::string& path) const;

    // File I/O
    std::vector<char> readFile(const std::string& path, size_t offset, size_t size);
    ssize_t writeFile(const std::string& path, const char* buf, size_t size, size_t offset);

    // File lifecycle
    bool createFile(const std::string& path, mode_t mode);
    bool resizeFile(const std::string& path, size_t newSize);
    bool removeFile(const std::string& path);
    bool moveFile(const std::string& oldPath, const std::string& newPath);

    // Directory lifecycle
    bool makeDirectory(const std::string& path, mode_t mode);
    bool removeDirectory(const std::string& path);

    // Metadata mutation
    bool updateTimestamps(const std::string& path, time_t atime, time_t mtime);
    bool chmod(const std::string& path, mode_t mode);
    bool chown(const std::string& path, uid_t uid, gid_t gid);

    // Filesystem stats
    [[nodiscard]] size_t getTotalBlocks() const;
    [[nodiscard]] size_t getFreeBlocks() const;
};

} // namespace vh::shared::bridge

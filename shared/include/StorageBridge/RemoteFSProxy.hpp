#pragma once

#include <memory>
#include <string>
#include <vector>

namespace vh::types {
struct File;
}

namespace vh::shared::bridge {

class UnifiedStorage;

class RemoteFSProxy {
  public:
    explicit RemoteFSProxy(std::shared_ptr<UnifiedStorage> backend);

    // File and Directory Queries
    [[nodiscard]] bool fileExists(const std::string& path) const;
    [[nodiscard]] types::File stat(const std::string& path) const;
    [[nodiscard]] std::vector<types::File> listDirectory(const std::string& path) const;

    // File I/O
    ssize_t readFile(const std::string& path, char* buf, size_t size, off_t offset);
    ssize_t writeFile(const std::string& path, const char* buf, size_t size, off_t offset);

    // File Operations
    bool createFile(const std::string& path, mode_t mode);
    bool resizeFile(const std::string& path, size_t newSize);
    bool deleteFile(const std::string& path);
    bool rename(const std::string& oldPath, const std::string& newPath);

    // Directory Operations
    bool mkdir(const std::string& path, mode_t mode);
    bool deleteDirectory(const std::string& path);

    // Metadata Mutations
    bool updateTimestamps(const std::string& path, time_t atime, time_t mtime);
    bool setPermissions(const std::string& path, mode_t mode);
    bool setOwnership(const std::string& path, uid_t uid, gid_t gid);

    // Filesystem Stats
    [[nodiscard]] size_t getTotalBlocks() const;
    [[nodiscard]] size_t getFreeBlocks() const;

  private:
    std::shared_ptr<UnifiedStorage> backend_;
};

} // namespace vh::shared::bridge

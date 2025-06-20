#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace vh::storage {

class StorageEngine {
  public:
    explicit StorageEngine(const fs::path& root_directory);

    StorageEngine() = default;
    virtual ~StorageEngine() = default;

    // Concrete methods for convenience
    bool writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data);
    [[nodiscard]] std::vector<fs::path> listFilesInDir(const fs::path& relative_path) const;

    // Virtual methods to be implemented by derived classes
    virtual bool writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data, bool overwrite) = 0;
    [[nodiscard]] virtual std::optional<std::vector<uint8_t>> readFile(const fs::path& relative_path) const = 0;
    virtual bool deleteFile(const fs::path& relative_path) = 0;
    [[nodiscard]] virtual bool fileExists(const fs::path& relative_path) const = 0;
    [[nodiscard]] virtual std::vector<fs::path> listFilesInDir(const fs::path& relative_path, bool recursive) const = 0;
    [[nodiscard]] virtual fs::path resolvePath(const std::string& id) const = 0;
    [[nodiscard]] virtual fs::path getRelativePath(const fs::path& absolute_path) const = 0;
    [[nodiscard]] virtual fs::path getAbsolutePath(const fs::path& relative_path) const = 0;
    [[nodiscard]] virtual fs::path getRootPath() const;

  private:
    fs::path root;
};

} // namespace vh::storage

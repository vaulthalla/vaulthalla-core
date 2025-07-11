#pragma once

#include <filesystem>
#include <optional>
#include <utility>
#include <vector>
#include <memory>

namespace fs = std::filesystem;

namespace vh::types {
struct Vault;
struct File;
}

namespace vh::storage {

enum class StorageType { Local, Cloud };

class StorageEngine {
public:
    StorageEngine() = default;

    explicit StorageEngine(const std::shared_ptr<types::Vault>& vault, fs::path root_mount_path = fs::path());

    virtual ~StorageEngine() = default;

    virtual void finishUpload(const std::filesystem::path& rel_path, const std::string& mime_type) = 0;

    virtual void mkdir(const fs::path& relative_path) = 0;

    [[nodiscard]] virtual std::optional<std::vector<uint8_t> > readFile(const fs::path& relative_path) const = 0;

    virtual void deleteFile(const fs::path& relative_path) = 0;

    [[nodiscard]] virtual bool fileExists(const fs::path& relative_path) const = 0;

    [[nodiscard]] virtual bool isDirectory(const fs::path& rel_path) const = 0;

    [[nodiscard]] virtual bool isFile(const fs::path& rel_path) const = 0;

    [[nodiscard]] virtual std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const = 0;

    [[nodiscard]] std::filesystem::path getAbsoluteCachePath(const std::filesystem::path& rel_path,
                                                             const std::filesystem::path& prefix = {}) const;

    [[nodiscard]] std::shared_ptr<types::Vault> getVault() const { return vault_; }

    [[nodiscard]] fs::path root_directory() const { return root_; }

    [[nodiscard]] virtual StorageType type() const = 0;

protected:
    std::shared_ptr<types::Vault> vault_;
    fs::path cache_path_, root_;
};

} // namespace vh::storage
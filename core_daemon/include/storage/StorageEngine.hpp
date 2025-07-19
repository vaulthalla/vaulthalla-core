#pragma once

#include <filesystem>
#include <optional>
#include <utility>
#include <vector>
#include <memory>
#include <shared_mutex>

namespace fs = std::filesystem;

namespace vh::types {
struct Vault;
struct File;
struct Sync;
}

namespace vh::concurrency {
class SyncTask;
class ThumbnailWorker;
}

namespace vh::storage {

enum class StorageType { Local, Cloud };

class StorageEngine : public std::enable_shared_from_this<StorageEngine> {
public:
    std::shared_ptr<types::Sync> sync_;
    std::shared_mutex mutex_;

    static constexpr uintmax_t MIN_FREE_SPACE = 10 * 1024 * 1024; // 10 MB

    StorageEngine() = default;

    explicit StorageEngine(const std::shared_ptr<types::Vault>& vault,
                           const std::shared_ptr<types::Sync>& sync,
                           const std::shared_ptr<concurrency::ThumbnailWorker>& thumbnailWorker,
                           fs::path root_mount_path = fs::path());

    virtual ~StorageEngine() = default;

    virtual void finishUpload(unsigned int userId, const std::filesystem::path& relPath) = 0;

    virtual void mkdir(const fs::path& relative_path) = 0;

    void move(const fs::path& from, const fs::path& to, unsigned int userId) const;

    void rename(const fs::path& from, const fs::path& to, unsigned int userId) const;

    void copy(const fs::path& from, const fs::path& to, unsigned int userId) const;

    [[nodiscard]] virtual std::optional<std::vector<uint8_t> > readFile(const fs::path& relative_path) const = 0;

    void remove(const fs::path& rel_path, unsigned int userId) const;

    [[nodiscard]] virtual bool fileExists(const fs::path& relative_path) const = 0;

    [[nodiscard]] bool isDirectory(const fs::path& rel_path) const;

    [[nodiscard]] bool isFile(const fs::path& rel_path) const;

    [[nodiscard]] virtual std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const;

    [[nodiscard]] std::filesystem::path getRelativePath(const std::filesystem::path& abs_path) const;

    [[nodiscard]] static uintmax_t getDirectorySize(const std::filesystem::path& path);

    [[nodiscard]] uintmax_t getVaultSize() const;

    [[nodiscard]] uintmax_t getCacheSize() const;

    [[nodiscard]] uintmax_t getVaultAndCacheTotalSize() const;

    [[nodiscard]] uintmax_t freeSpace() const;

    [[nodiscard]] std::shared_ptr<types::File> createFile(const std::filesystem::path& rel_path,
                                                          const std::filesystem::path& abs_path = {}) const;

    [[nodiscard]] std::filesystem::path getAbsoluteCachePath(const std::filesystem::path& rel_path,
                                                             const std::filesystem::path& prefix = {}) const;

    [[nodiscard]] std::filesystem::path getRelativeCachePath(const std::filesystem::path& abs_path) const;

    void writeFile(const std::filesystem::path& abs_path, const std::string& buffer) const;

    [[nodiscard]] std::shared_ptr<types::Vault> getVault() const { return vault_; }

    [[nodiscard]] unsigned int vaultId() const;

    [[nodiscard]] fs::path root_directory() const { return root_; }

    [[nodiscard]] virtual StorageType type() const = 0;

    void purgeThumbnails(const fs::path& rel_path) const;

    static std::string getMimeType(const std::filesystem::path& path);

protected:
    std::shared_ptr<types::Vault> vault_;
    fs::path cache_path_, root_;
    std::shared_ptr<concurrency::ThumbnailWorker> thumbnailWorker_;

    void removeFile(const fs::path& rel_path, unsigned int userId) const;
    void removeDirectory(const fs::path& rel_path, unsigned int userId) const;

    friend class concurrency::SyncTask;
};

} // namespace vh::storage
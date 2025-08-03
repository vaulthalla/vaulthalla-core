#pragma once

#include <filesystem>
#include <vector>
#include <memory>
#include <shared_mutex>

namespace fs = std::filesystem;

namespace vh::types {
struct Vault;
struct File;
struct Sync;
struct Path;
}

namespace vh::encryption {
class VaultEncryptionManager;
}

namespace vh::storage {

enum class StorageType { Local, Cloud };

struct StorageEngine : public std::enable_shared_from_this<StorageEngine> {
    std::shared_ptr<types::Vault> vault;
    std::shared_ptr<types::Sync> sync;
    std::shared_ptr<types::Path> paths;
    std::shared_ptr<encryption::VaultEncryptionManager> encryptionManager;
    std::shared_mutex mutex;

    static constexpr uintmax_t MIN_FREE_SPACE = 10 * 1024 * 1024; // 10 MB

    StorageEngine() = default;

    explicit StorageEngine(const std::shared_ptr<types::Vault>& vault);

    virtual ~StorageEngine() = default;

    [[nodiscard]] bool isDirectory(const fs::path& rel_path) const;

    [[nodiscard]] bool isFile(const fs::path& rel_path) const;

    [[nodiscard]] std::vector<uint8_t> decrypt(unsigned int vaultId, const std::filesystem::path& relPath,
                                               const std::vector<uint8_t>& payload) const;

    void mkdir(const fs::path& relPath, unsigned int userId);
    void move(const fs::path& from, const fs::path& to, unsigned int userId);
    void rename(const fs::path& from, const fs::path& to, unsigned int userId);
    void copy(const fs::path& from, const fs::path& to, unsigned int userId);
    void remove(const fs::path& rel_path, unsigned int userId);

    [[nodiscard]] static uintmax_t getDirectorySize(const fs::path& path);
    [[nodiscard]] uintmax_t getVaultSize() const;
    [[nodiscard]] uintmax_t getCacheSize() const;
    [[nodiscard]] uintmax_t getVaultAndCacheTotalSize() const;
    [[nodiscard]] uintmax_t freeSpace() const;

    [[nodiscard]] virtual StorageType type() const { return StorageType::Local; }

    void purgeThumbnails(const fs::path& rel_path) const;
    void moveThumbnails(const fs::path& from, const fs::path& to) const;
    void copyThumbnails(const fs::path& from, const fs::path& to) const;
};

} // namespace vh::storage
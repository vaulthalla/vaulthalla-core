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
}

namespace vh::encryption {
class VaultEncryptionManager;
}

namespace vh::engine {

enum class StorageType { Local, Cloud };

struct StorageEngineBase : public std::enable_shared_from_this<StorageEngineBase> {
    std::shared_ptr<types::Vault> vault;
    std::shared_ptr<types::Sync> sync;
    fs::path cacheRoot, root;
    std::shared_ptr<encryption::VaultEncryptionManager> encryptionManager;
    std::shared_mutex mutex;

    static constexpr uintmax_t MIN_FREE_SPACE = 10 * 1024 * 1024; // 10 MB

    StorageEngineBase() = default;

    explicit StorageEngineBase(const std::shared_ptr<types::Vault>& vault);

    virtual ~StorageEngineBase() = default;

    [[nodiscard]] bool isDirectory(const fs::path& rel_path) const;

    [[nodiscard]] bool isFile(const fs::path& rel_path) const;

    [[nodiscard]] virtual std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const;

    [[nodiscard]] std::filesystem::path getRelativePath(const std::filesystem::path& abs_path) const;

    [[nodiscard]] std::filesystem::path getAbsoluteCachePath(const std::filesystem::path& rel_path,
                                                             const std::filesystem::path& prefix = {}) const;

    [[nodiscard]] std::filesystem::path getRelativeCachePath(const std::filesystem::path& abs_path) const;

    [[nodiscard]] virtual StorageType type() const = 0;

    [[nodiscard]] std::shared_ptr<types::File> createFile(const fs::path& rel_path,
                                                          const std::vector<uint8_t>& = {}) const;

    [[nodiscard]] std::vector<uint8_t> decrypt(unsigned int vaultId, const std::filesystem::path& relPath,
                                               const std::vector<uint8_t>& payload) const;
};

} // namespace vh::storage
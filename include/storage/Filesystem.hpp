#pragma once

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>

namespace fs = std::filesystem;

namespace vh::types {
    struct FSEntry;
}

namespace vh::storage {

class StorageManager;
class StorageEngine;

class Filesystem {
public:
    static void init(std::shared_ptr<StorageManager> manager);
    static bool isReady();
    static void mkdir(const fs::path& absPath, mode_t mode = 0755, const std::optional<unsigned int>& userId = std::nullopt, std::shared_ptr<StorageEngine> engine = nullptr);
    static void mkVault(const fs::path& absPath, unsigned int vaultId, mode_t mode = 0755);
    static void mkCache(const fs::path& absPath, mode_t mode = 0755);
    static bool exists(const fs::path& absPath);

    static void copy(const fs::path& from, const fs::path& to, unsigned int userId, std::shared_ptr<StorageEngine> engine = nullptr);
    static void remove(const fs::path& path, unsigned int userId, std::shared_ptr<StorageEngine> engine = nullptr);

    static std::shared_ptr<types::FSEntry> createFile(const fs::path& path, uid_t uid, gid_t gid, mode_t mode = 0644);
    static void renamePath(const fs::path& oldPath, const fs::path& newPath, const std::optional<unsigned int>& userId = std::nullopt, std::shared_ptr<StorageEngine> engine = nullptr);
    static void updatePaths(const fs::path& oldPath, const fs::path& newPath, const std::optional<unsigned int>& userId = std::nullopt, std::shared_ptr<StorageEngine> engine = nullptr);

private:
    inline static std::mutex mutex_;
    inline static std::shared_ptr<StorageManager> storageManager_ = nullptr;
};

}

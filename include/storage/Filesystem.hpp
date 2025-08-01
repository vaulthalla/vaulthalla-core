#pragma once

#include <filesystem>
#include <memory>
#include <mutex>

namespace fs = std::filesystem;

namespace vh::types {
    struct FSEntry;
}

namespace vh::storage {

class StorageManager;

class Filesystem {
public:
    static void init(std::shared_ptr<StorageManager> manager);
    static bool isReady();
    static void mkdir(const fs::path& absPath, mode_t mode = 0755);
    static void mkVault(const fs::path& absPath, unsigned int vaultId, mode_t mode = 0755);
    static void mkCache(const fs::path& absPath, mode_t mode = 0755);
    static bool exists(const fs::path& absPath);

    static std::shared_ptr<types::FSEntry> createFile(const fs::path& path, uid_t uid, gid_t gid, mode_t mode = 0644);
    static void renamePath(const fs::path& oldPath, const fs::path& newPath);
    static void updatePaths(const fs::path& oldPath, const fs::path& newPath);

private:
    inline static std::mutex mutex_;
    inline static std::shared_ptr<StorageManager> storageManager_ = nullptr;
};

}

#pragma once

#include <filesystem>
#include <memory>
#include <mutex>

namespace fs = std::filesystem;

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

private:
    inline static std::mutex mutex_;
    inline static std::shared_ptr<StorageManager> storageManager_ = nullptr;
};

}

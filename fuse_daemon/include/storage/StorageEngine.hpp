#pragma once

#include "engine/StorageEngineBase.hpp"

#include <filesystem>
#include <memory>

namespace fs = std::filesystem;

namespace vh::types {
struct Vault;
}

namespace vh::storage {

struct StorageEngine : engine::StorageEngineBase {

    StorageEngine() = default;
    ~StorageEngine() override = default;

    explicit StorageEngine(const std::shared_ptr<types::Vault>& vault);

    [[nodiscard]] static uintmax_t getDirectorySize(const fs::path& path);

    [[nodiscard]] uintmax_t getVaultSize() const;

    [[nodiscard]] uintmax_t getCacheSize() const;

    [[nodiscard]] uintmax_t getVaultAndCacheTotalSize() const;

    [[nodiscard]] uintmax_t freeSpace() const;

    [[nodiscard]] virtual engine::StorageType type() const { return engine::StorageType::Local; }

    void purgeThumbnails(const fs::path& rel_path) const;

    void moveThumbnails(const fs::path& from, const fs::path& to) const;

    void copyThumbnails(const fs::path& from, const fs::path& to) const;

    static std::string getMimeType(const fs::path& path);
};

} // namespace vh::storage
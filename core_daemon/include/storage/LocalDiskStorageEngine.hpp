#pragma once

#include "storage/StorageEngine.hpp"
#include <filesystem>
#include <memory>

namespace vh::types {
struct LocalDiskVault;
struct File;
}

namespace vh::storage {

class LocalDiskStorageEngine : public StorageEngine {
public:
    explicit LocalDiskStorageEngine(const std::shared_ptr<concurrency::ThumbnailWorker>& thumbnailWorker,
                                    const std::shared_ptr<types::Sync>& sync,
                                    const std::shared_ptr<types::LocalDiskVault>& vault);

    ~LocalDiskStorageEngine() override = default;

    [[nodiscard]] StorageType type() const override { return StorageType::Local; }

    void finishUpload(unsigned int userId, const std::filesystem::path& relPath) override;

    [[nodiscard]] std::optional<std::vector<uint8_t> > readFile(const std::filesystem::path& rel_path) const override;

    [[nodiscard]] std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const override;

    [[nodiscard]] std::filesystem::path getRootPath() const;
};

} // namespace vh::storage
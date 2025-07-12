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
    explicit LocalDiskStorageEngine(const std::shared_ptr<services::ThumbnailWorker>& thumbnailWorker,
                                    const std::shared_ptr<types::LocalDiskVault>& vault);

    ~LocalDiskStorageEngine() override = default;

    [[nodiscard]] StorageType type() const override { return StorageType::Local; }

    void finishUpload(const std::filesystem::path& rel_path, const std::string& mime_type) override;

    void mkdir(const fs::path& relative_path) override;

    bool writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data, bool overwrite);

    [[nodiscard]] std::optional<std::vector<uint8_t> > readFile(const std::filesystem::path& rel_path) const override;

    void remove(const std::filesystem::path& rel_path) override;

    [[nodiscard]] bool fileExists(const std::filesystem::path& rel_path) const override;

    [[nodiscard]] bool isDirectory(const fs::path& rel_path) const override;

    [[nodiscard]] bool isFile(const fs::path& rel_path) const override;

    [[nodiscard]] std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const override;

    [[nodiscard]] std::filesystem::path getRootPath() const;

    [[nodiscard]] fs::path resolvePath(const std::string& id) const;

    [[nodiscard]] fs::path getRelativePath(const fs::path& absolute_path) const;

protected:

    void removeFile(const fs::path& rel_path) override;
    void removeDirectory(const fs::path& rel_path) override;
};

} // namespace vh::storage
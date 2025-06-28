#pragma once

#include "storage/StorageEngine.hpp"
#include <filesystem>
#include <memory>

namespace vh::types {
struct LocalDiskVault;
struct Volume;
struct File;
}

namespace vh::storage {

class LocalDiskStorageEngine : public StorageEngine {
  public:
    LocalDiskStorageEngine(const std::shared_ptr<types::LocalDiskVault>& vault,
                           const std::vector<std::shared_ptr<types::Volume>>& volumes);
    ~LocalDiskStorageEngine() override = default;

    void mountVolume(const std::shared_ptr<types::Volume>& volume) override;
    void unmountVolume(const std::shared_ptr<types::Volume>& volume) override;

    [[nodiscard]] StorageType type() const override { return StorageType::Local; }

    bool writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data, bool overwrite) override;
    [[nodiscard]] std::optional<std::vector<uint8_t>> readFile(const std::filesystem::path& rel_path) const override;
    bool deleteFile(const std::filesystem::path& rel_path) override;
    [[nodiscard]] bool fileExists(const std::filesystem::path& rel_path) const override;

    std::vector<std::shared_ptr<types::File>> listFilesInDir(const std::filesystem::path& rel_path,
                                                                    bool recursive) const override;

    [[nodiscard]] std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const;
    [[nodiscard]] std::filesystem::path getRootPath() const;
    [[nodiscard]] fs::path resolvePath(const std::string& id) const;
    [[nodiscard]] fs::path getRelativePath(const fs::path& absolute_path) const;

  private:
    std::shared_ptr<types::Vault> vault_;
    std::vector<std::shared_ptr<types::Volume>> volumes_;
    std::filesystem::path root;
};

} // namespace vh::storage

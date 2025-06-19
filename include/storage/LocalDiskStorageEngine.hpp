#pragma once

#include "include/storage/StorageEngine.hpp"
#include "../../shared/include/types/StorageVolume.hpp"
#include <filesystem>
#include <fstream>

namespace vh::storage {

    class LocalDiskStorageEngine : public StorageEngine {
    public:
        explicit LocalDiskStorageEngine(std::filesystem::path root_dir);
        ~LocalDiskStorageEngine() override = default;

        void mountVolume(const std::filesystem::path& mount_point);
        void unmountVolume(const vh::types::StorageVolume& volume);

        bool writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data, bool overwrite) override;
        [[nodiscard]] std::optional<std::vector<uint8_t>> readFile(const std::filesystem::path& rel_path) const override;
        bool deleteFile(const std::filesystem::path& rel_path) override;
        [[nodiscard]] bool fileExists(const std::filesystem::path& rel_path) const override;
        [[nodiscard]] std::vector<std::filesystem::path> listFilesInDir(const std::filesystem::path& rel_path, bool recursive) const override;
        [[nodiscard]] std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const override;
        [[nodiscard]] std::filesystem::path getRootPath() const override;
        [[nodiscard]] fs::path resolvePath(const std::string& id) const override;
        [[nodiscard]] fs::path getRelativePath(const fs::path& absolute_path) const;

    private:
        std::filesystem::path root;
    };

} // namespace vh::storage

#pragma once

#include "storage/StorageEngine.hpp"

namespace vh::storage {

    class CloudStorageEngine : public StorageEngine {
    public:
        CloudStorageEngine() = default;
        ~CloudStorageEngine() override = default;

        bool writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data, bool overwrite) override;
        [[nodiscard]] std::optional<std::vector<uint8_t>> readFile(const std::filesystem::path& rel_path) const override;
        bool deleteFile(const std::filesystem::path& rel_path) override;
        [[nodiscard]] bool fileExists(const std::filesystem::path& rel_path) const override;
        [[nodiscard]] std::vector<std::filesystem::path> listFilesInDir(const std::filesystem::path& rel_path, bool recursive) const override;
        [[nodiscard]] std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const override;
        [[nodiscard]] std::filesystem::path getRootPath() const override;
        [[nodiscard]] fs::path resolvePath(const std::string& id) const override;
        [[nodiscard]] fs::path getRelativePath(const fs::path& absolute_path) const override;
    };

} // namespace vh::storage

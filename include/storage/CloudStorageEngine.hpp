#pragma once

#include "storage/StorageEngine.hpp"

namespace vh::storage {

    class CloudStorageEngine : public StorageEngine {
    public:
        CloudStorageEngine(); // No root path for cloud → override getRootPath()

        ~CloudStorageEngine() override = default;

        bool writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data, bool overwrite) override;
        bool writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data) override;
        [[nodiscard]] std::optional<std::vector<uint8_t>> readFile(const std::filesystem::path& rel_path) const override;
        bool deleteFile(const std::filesystem::path& rel_path) override;
        [[nodiscard]] bool fileExists(const std::filesystem::path& rel_path) const override;
        std::vector<std::filesystem::path> listFilesInDir(const std::filesystem::path& rel_path, bool recursive) const override;
        [[nodiscard]] std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const override;
        [[nodiscard]] std::filesystem::path getRootPath() const override;
    };

} // namespace vh::storage

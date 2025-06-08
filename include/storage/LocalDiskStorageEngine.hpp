#pragma once

#include "include/storage/StorageEngine.hpp"
#include <filesystem>
#include <fstream>

namespace vh::storage {

    class LocalDiskStorageEngine : public StorageEngine {
    public:
        explicit LocalDiskStorageEngine(std::filesystem::path  root_dir);
        ~LocalDiskStorageEngine() override = default;

        bool writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data, bool overwrite) override;
        bool writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data) override;
        [[nodiscard]] std::optional<std::vector<uint8_t>> readFile(const std::filesystem::path& rel_path) const override;
        bool deleteFile(const std::filesystem::path& rel_path) override;
        [[nodiscard]] bool fileExists(const std::filesystem::path& rel_path) const override;
        std::vector<std::filesystem::path> listFilesInDir(const std::filesystem::path& rel_path, bool recursive = false) const override;
        [[nodiscard]] std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const override;
        [[nodiscard]] std::filesystem::path getRootPath() const override;

    private:
        std::filesystem::path root;
    };

} // namespace vh::storage

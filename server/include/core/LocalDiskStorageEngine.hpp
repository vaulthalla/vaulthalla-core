#pragma once

#include "StorageEngine.hpp"
#include <filesystem>
#include <fstream>

namespace core {

    class LocalDiskStorageEngine : public StorageEngine {
    public:
        explicit LocalDiskStorageEngine(std::filesystem::path  root_dir);
        ~LocalDiskStorageEngine() override = default;

        bool writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data, bool overwrite) override;
        [[nodiscard]] std::optional<std::vector<uint8_t>> readFile(const std::filesystem::path& rel_path) const override;
        bool deleteFile(const std::filesystem::path& rel_path) override;

    private:
        std::filesystem::path root;
    };

}

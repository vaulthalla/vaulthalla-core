#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <optional>
#include <cstdint>

namespace fs = std::filesystem;

namespace core {

    class StorageEngine {
    public:
        explicit StorageEngine(const fs::path& root_directory);

        StorageEngine() = default;
        virtual ~StorageEngine() = default;

        virtual bool writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data, bool overwrite);
        [[nodiscard]] virtual std::optional<std::vector<uint8_t>> readFile(const fs::path& relative_path) const;

        virtual bool deleteFile(const fs::path& relative_path);
        [[nodiscard]] virtual bool fileExists(const fs::path& relative_path) const;

        [[nodiscard]] fs::path getAbsolutePath(const fs::path& relative_path) const;

    private:
        fs::path root;
    };

} // namespace core

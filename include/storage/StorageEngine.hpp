#pragma once

#include <string>
#include <filesystem>
#include <vector>
#include <optional>
#include <cstdint>

namespace fs = std::filesystem;

namespace vh::storage {

    class StorageEngine {
    public:
        explicit StorageEngine(const fs::path& root_directory);

        StorageEngine() = default;
        virtual ~StorageEngine() = default;

        virtual bool writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data, bool overwrite);
        virtual bool writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data);
        [[nodiscard]] virtual std::optional<std::vector<uint8_t>> readFile(const fs::path& relative_path) const;

        virtual bool deleteFile(const fs::path& relative_path);
        [[nodiscard]] virtual bool fileExists(const fs::path& relative_path) const;

        virtual std::vector<fs::path> listFilesInDir(const fs::path& relative_path, bool recursive) const;

        std::vector<fs::path> listFilesInDir(const fs::path& relative_path) const;

        [[nodiscard]] virtual fs::path getAbsolutePath(const fs::path& relative_path) const;
        [[nodiscard]] virtual fs::path getRootPath() const;

    private:
        fs::path root;
    };

} // namespace vh::storage

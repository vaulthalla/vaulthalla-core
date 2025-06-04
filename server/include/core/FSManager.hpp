#pragma once

#include <string>
#include <unordered_map>
#include <filesystem>
#include <optional>
#include <memory>
#include "core/StorageEngine.hpp"
#include "core/FileMetadata.hpp"

namespace fs = std::filesystem;

namespace core {

    class FSManager {
    public:
        explicit FSManager(const fs::path& root_directory);
        explicit FSManager(const std::shared_ptr<StorageEngine>& storage_engine);

        bool saveFile(const std::string& id, const std::vector<uint8_t>& data);
        [[nodiscard]] std::optional<std::vector<uint8_t>> loadFile(const std::string& id) const;
        bool deleteFile(const std::string& id);
        [[nodiscard]] bool fileExists(const std::string& id) const;

        std::unordered_map<std::string, FileMetadata> getIndex() const;
        [[nodiscard]] std::optional<FileMetadata> getMetadata(const std::string& id) const;

    private:
        std::shared_ptr<StorageEngine> storage;
        std::unordered_map<std::string, FileMetadata> file_index;

        [[nodiscard]] fs::path resolvePath(const std::string& id) const;
    };

} // namespace core

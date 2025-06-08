#pragma once

#include "FileMetadata.hpp"
#include "DirectoryWalker.hpp"
#include "index/FileScanner.hpp"
#include "index/SearchIndex.hpp"

#include <filesystem>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

namespace vh::storage {
    class StorageEngine;
}

namespace vh::core {

    class FSManager {
    public:
        explicit FSManager(const std::filesystem::path& root_directory);
        explicit FSManager(const std::shared_ptr<vh::storage::StorageEngine>& storage_engine);

        // Core file APIs
        std::filesystem::path resolvePath(const std::string& id) const;
        bool saveFile(const std::string& id, const std::vector<uint8_t>& data);
        std::optional<std::vector<uint8_t>> loadFile(const std::string& id) const;
        bool deleteFile(const std::string& id);
        bool fileExists(const std::string& id) const;
        std::optional<FileMetadata> getMetadata(const std::string& id) const;
        std::unordered_map<std::string, FileMetadata> getIndex() const;

        // New APIs for full FS control
        void rebuildIndex(bool recursive = true);
        std::vector<std::filesystem::path> search(const std::string& term) const;
        void scanFile(const std::filesystem::path& path);
        std::vector<std::filesystem::path> listFilesInDir(const std::filesystem::path& dir, bool recursive = true) const;

        std::shared_ptr<vh::storage::StorageEngine> getStorageEngine() const { return storage; }

    private:
        std::shared_ptr<vh::storage::StorageEngine> storage;
        std::unordered_map<std::string, FileMetadata> file_index;

        // Injected tools
        core::DirectoryWalker directoryWalker{true};
        index::FileScanner fileScanner;
        index::SearchIndex searchIndex;
    };

} // namespace vh::core

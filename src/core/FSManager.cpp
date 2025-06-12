#include "core/FSManager.hpp"
#include "include/storage/LocalDiskStorageEngine.hpp"
#include <iostream>

namespace vh::core {

    FSManager::FSManager()
            : storageEngine_(std::make_shared<vh::storage::LocalDiskStorageEngine>(rootDirectory_)),
              directoryWalker_(true) {
        if (!std::filesystem::exists(rootDirectory_)) {
            std::cout << "[FSManager] Root directory does not exist, creating: " << rootDirectory_ << "\n";
            std::filesystem::create_directories(rootDirectory_);
        }

        // TODO: Load index from disk (future enhancement)
    }

    FSManager::FSManager(const std::shared_ptr<vh::storage::LocalDiskStorageEngine>& storage_engine)
            : rootDirectory_(storage_engine->getRootPath()),
              storageEngine_(storage_engine),
              directoryWalker_(true) {}

    fs::path FSManager::resolvePath(const std::string& id) const {
        if (id.empty()) throw std::invalid_argument("File ID cannot be empty");

        fs::path rel_path = fs::path(id);
        if (rel_path.is_absolute() || rel_path.has_root_name())
            throw std::invalid_argument("File ID must be a relative path");

        return rootDirectory_ / rel_path;
    }

    bool FSManager::saveFile(const std::string& id, const std::vector<uint8_t>& data) {
        fs::path rel_path = resolvePath(id);
        fs::create_directories(rel_path.parent_path());
        if (!storageEngine_->writeFile(rel_path, data, false)) return false;

        FileMetadata meta(id, rel_path, data.size());
        fileIndex_[id] = meta;
        return true;
    }

    std::optional<std::vector<uint8_t>> FSManager::loadFile(const std::string& id) const {
        fs::path rel_path = resolvePath(id);
        return storageEngine_->readFile(rel_path);
    }

    bool FSManager::deleteFile(const std::string& id) {
        fs::path rel_path = resolvePath(id);
        if (!storageEngine_->deleteFile(rel_path)) return false;

        fileIndex_.erase(id);
        return true;
    }

    bool FSManager::fileExists(const std::string& id) const {
        return storageEngine_->fileExists(resolvePath(id));
    }

    std::optional<FileMetadata> FSManager::getMetadata(const std::string& id) const {
        auto it = fileIndex_.find(id);
        if (it != fileIndex_.end()) return it->second;
        return std::nullopt;
    }

    std::unordered_map<std::string, FileMetadata> FSManager::getIndex() const {
        return fileIndex_;
    }

    void FSManager::rebuildIndex(bool recursive) {
        directoryWalker_ = core::DirectoryWalker(recursive);
        searchIndex_ = index::SearchIndex(); // Reset search index

        auto entries = directoryWalker_.walk(storageEngine_->getRootPath(), [](const auto& entry) {
            return entry.is_regular_file();
        });

        for (const auto& entry : entries) {
            auto metadata = fileScanner_.scan(entry.path);
            std::cout << "Scanning: " << entry.path << " | isText: " << metadata.isTextFile << "\n";
            searchIndex_.addToIndex(entry.path, metadata.contentPreview.value_or(""));
        }

        std::cout << "Rebuilt search index with " << entries.size() << " files.\n";
    }

    std::vector<std::filesystem::path> FSManager::search(const std::string& term) const {
        return searchIndex_.search(term);
    }

    void FSManager::scanFile(const std::filesystem::path& path) {
        auto metadata = fileScanner_.scan(path);
        searchIndex_.addToIndex(path, metadata.contentPreview.value_or(""));
        std::cout << "Scanned & indexed: " << path << "\n";
    }

    std::vector<std::filesystem::path> FSManager::listFilesInDir(const std::filesystem::path& dir, bool recursive) const {
        core::DirectoryWalker walker(recursive);
        auto entries = walker.walk(dir, [](const auto& entry) {
            return entry.is_regular_file();
        });

        std::vector<std::filesystem::path> paths;
        for (const auto& entry : entries) {
            paths.push_back(entry.path);
        }

        return paths;
    }

} // namespace vh::core

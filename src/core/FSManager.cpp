#include "core/FSManager.hpp"
#include <iostream>

namespace vh::core {

    FSManager::FSManager(const std::filesystem::path& root_directory)
            : storage(std::make_shared<StorageEngine>(root_directory)),
              directoryWalker(true) {
        // TODO: Load index from disk (future enhancement)
    }

    FSManager::FSManager(const std::shared_ptr<StorageEngine>& storage_engine)
            : storage(storage_engine),
              directoryWalker(true) {}

    fs::path FSManager::resolvePath(const std::string& id) const {
        return fs::path("data") / (id + ".bin");  // can expand to hashed buckets later
    }

    bool FSManager::saveFile(const std::string& id, const std::vector<uint8_t>& data) {
        fs::path rel_path = resolvePath(id);
        fs::create_directories(rel_path.parent_path());
        if (!storage->writeFile(rel_path, data, false)) return false;

        FileMetadata meta(id, rel_path, data.size());
        file_index[id] = meta;
        return true;
    }

    std::optional<std::vector<uint8_t>> FSManager::loadFile(const std::string& id) const {
        fs::path rel_path = resolvePath(id);
        return storage->readFile(rel_path);
    }

    bool FSManager::deleteFile(const std::string& id) {
        fs::path rel_path = resolvePath(id);
        if (!storage->deleteFile(rel_path)) return false;

        file_index.erase(id);
        return true;
    }

    bool FSManager::fileExists(const std::string& id) const {
        return storage->fileExists(resolvePath(id));
    }

    std::optional<FileMetadata> FSManager::getMetadata(const std::string& id) const {
        auto it = file_index.find(id);
        if (it != file_index.end()) return it->second;
        return std::nullopt;
    }

    std::unordered_map<std::string, FileMetadata> FSManager::getIndex() const {
        return file_index;
    }

    void FSManager::rebuildIndex(bool recursive) {
        directoryWalker = core::DirectoryWalker(recursive);
        searchIndex = index::SearchIndex(); // Reset search index

        auto entries = directoryWalker.walk(storage->getRootPath(), [](const auto& entry) {
            return entry.is_regular_file();
        });

        for (const auto& entry : entries) {
            auto metadata = fileScanner.scan(entry.path);
            std::cout << "Scanning: " << entry.path << " | isText: " << metadata.isTextFile << "\n";
            searchIndex.addToIndex(entry.path, metadata.contentPreview.value_or(""));
        }

        std::cout << "Rebuilt search index with " << entries.size() << " files.\n";
    }

    std::vector<std::filesystem::path> FSManager::search(const std::string& term) const {
        return searchIndex.search(term);
    }

    void FSManager::scanFile(const std::filesystem::path& path) {
        auto metadata = fileScanner.scan(path);
        searchIndex.addToIndex(path, metadata.contentPreview.value_or(""));
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

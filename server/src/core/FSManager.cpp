#include "core/FSManager.hpp"
#include <iostream>

namespace core {

    FSManager::FSManager(const fs::path& root_directory)
            : storage(std::make_unique<StorageEngine>(root_directory)) {
        // TODO: Load index from persistent store or database
    }

    FSManager::FSManager(const std::shared_ptr<StorageEngine> &storage_engine) : storage(storage_engine) {}

    fs::path FSManager::resolvePath(const std::string& id) const {
        return fs::path("data") / (id + ".bin");  // can expand to hashed buckets later
    }

    bool FSManager::saveFile(const std::string& id, const std::vector<uint8_t>& data) {
        fs::path rel_path = resolvePath(id);
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

} // namespace core

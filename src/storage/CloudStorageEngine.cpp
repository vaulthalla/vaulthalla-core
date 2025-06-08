#include "storage/CloudStorageEngine.hpp"

#include <iostream>

namespace vh::storage {

    bool CloudStorageEngine::writeFile(const std::filesystem::path& rel_path,
                                       const std::vector<uint8_t>& data,
                                       bool overwrite) {
        std::cout << "[CloudStorageEngine] writeFile called: " << rel_path << "\n";
        // TODO: Implement S3/R2 write logic
        return true;
    }

    std::optional<std::vector<uint8_t>> CloudStorageEngine::readFile(const std::filesystem::path& rel_path) const {
        std::cout << "[CloudStorageEngine] readFile called: " << rel_path << "\n";
        // TODO: Implement S3/R2 read logic
        return std::nullopt;
    }

    bool CloudStorageEngine::deleteFile(const std::filesystem::path& rel_path) {
        std::cout << "[CloudStorageEngine] deleteFile called: " << rel_path << "\n";
        // TODO: Implement S3/R2 delete logic
        return true;
    }

    bool CloudStorageEngine::fileExists(const std::filesystem::path& rel_path) const {
        std::cout << "[CloudStorageEngine] fileExists called: " << rel_path << "\n";
        // TODO: Implement S3/R2 fileExists logic
        return false;
    }

    std::vector<std::filesystem::path> CloudStorageEngine::listFilesInDir(const std::filesystem::path& rel_path, bool recursive) const {
        std::cout << "[CloudStorageEngine] listFilesInDir called: " << rel_path << "\n";
        // TODO: Implement S3/R2 listFilesInDir logic
        return std::vector<std::filesystem::path>{};
    }

    std::filesystem::path CloudStorageEngine::resolvePath(const std::string& id) const {
        std::cout << "[CloudStorageEngine] resolvePath called: " << id << "\n";
        // Cloud paths don't have a real "resolve path" → return id for now
        return std::filesystem::path(id);
    }

    std::filesystem::path CloudStorageEngine::getAbsolutePath(const std::filesystem::path& rel_path) const {
        std::cout << "[CloudStorageEngine] getAbsolutePath called: " << rel_path << "\n";
        // Cloud paths don't have a real "absolute path" → return rel_path for now
        return rel_path;
    }

    std::filesystem::path CloudStorageEngine::getRelativePath(const std::filesystem::path& absolute_path) const {
        std::cout << "[CloudStorageEngine] getRelativePath called: " << absolute_path << "\n";
        // Cloud paths don't have a real "relative path" → return absolute_path for now
        return absolute_path;
    }

    std::filesystem::path CloudStorageEngine::getRootPath() const {
        // Cloud backend → no root path → return empty
        return std::filesystem::path{};
    }

} // namespace vh::storage

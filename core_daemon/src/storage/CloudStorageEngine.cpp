#include "storage/CloudStorageEngine.hpp"
#include "types/Vault.hpp"
#include <iostream>

namespace vh::storage {

CloudStorageEngine::CloudStorageEngine(const std::shared_ptr<types::Vault>& vault)
    : StorageEngine(vault) {
}

void CloudStorageEngine::mkdir(const std::filesystem::path& relative_path) {
    std::cout << "[CloudStorageEngine] mkdir called: " << relative_path << "\n";
}

bool CloudStorageEngine::writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data,
                                   bool overwrite) {
    std::cout << "[CloudStorageEngine] writeFile called: " << rel_path << "\n";
    // TODO: Implement S3/R2 write logic
    return true;
}

std::optional<std::vector<uint8_t> > CloudStorageEngine::readFile(const std::filesystem::path& rel_path) const {
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

} // namespace vh::storage
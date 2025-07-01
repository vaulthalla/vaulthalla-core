#include "storage/CloudStorageEngine.hpp"
#include "types/db/Vault.hpp"
#include "types/db/Volume.hpp"
#include <iostream>

namespace vh::storage {

CloudStorageEngine::CloudStorageEngine(const std::shared_ptr<types::Vault>& vault,
                                       const std::vector<std::shared_ptr<types::Volume>>& volumes)
    : StorageEngine(vault, volumes) {}

void CloudStorageEngine::mountVolume(const std::shared_ptr<types::Volume>& volume) {
    // TODO: Implement S3/R2 mount logic
}

void CloudStorageEngine::unmountVolume(const std::shared_ptr<types::Volume>& volume) {
    // TODO: Implement S3/R2 unmount logic
}

void CloudStorageEngine::mkdir(unsigned int volumeId, const std::filesystem::path& relative_path) {
    std::cout << "[CloudStorageEngine] mkdir called: " << relative_path << "\n";
}

bool CloudStorageEngine::writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data,
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

std::vector<std::shared_ptr<types::File>> CloudStorageEngine::listFilesInDir(unsigned int volume_id,
                                                                              const std::filesystem::path& rel_path,
                                                                              bool recursive) const {
    std::cout << "[CloudStorageEngine] listFilesInDir called: " << rel_path << "\n";
    return {};
}

} // namespace vh::storage

#include "shared/StorageBridge/UnifiedStorage.hpp"
#include "types/File.hpp"
#include <stdexcept>

using namespace vh::shared::bridge;

UnifiedStorage::UnifiedStorage() {
    // Init with LocalDiskStorageEngine or CloudStorageEngine
}

bool UnifiedStorage::exists(const std::string& path) const {
    // route to correct backend
    return false;
}

vh::types::File UnifiedStorage::getMetadata(const std::string& path) const {
    // mock or passthrough
    return vh::types::File{/* ... */};
}

std::vector<vh::types::File> UnifiedStorage::listDirectory(const std::string& path) const {
    return {};
}

std::vector<char> UnifiedStorage::readFile(const std::string& path, size_t offset, size_t size) {
    return {};
}

ssize_t UnifiedStorage::writeFile(const std::string& path, const char* buf, size_t size, size_t offset) {
    return -1;
}

bool UnifiedStorage::makeDirectory(const std::string& path) {
    return false;
}

bool UnifiedStorage::removeFile(const std::string& path) {
    return false;
}

bool UnifiedStorage::moveFile(const std::string& oldPath, const std::string& newPath) {
    return false;
}

#include "storage/StorageEngine.hpp"
#include <fstream>

namespace vh::storage {

StorageEngine::StorageEngine(const fs::path& root_directory) : root(fs::absolute(root_directory)) {
    if (!fs::exists(root)) fs::create_directories(root);
}

bool StorageEngine::writeFile(const fs::path& relative_path, const std::vector<uint8_t>& data) {
    return writeFile(relative_path, data, false);
}

std::vector<fs::path> StorageEngine::listFilesInDir(const fs::path& relative_path) const {
    return listFilesInDir(relative_path, false);
}

fs::path StorageEngine::getRootPath() const {
    return root;
}

} // namespace vh::storage

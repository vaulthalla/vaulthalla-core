#include "storage/LocalDiskStorageEngine.hpp"
#include <utility>

namespace vh::storage {

LocalDiskStorageEngine::LocalDiskStorageEngine(std::filesystem::path root_dir) : root(std::move(root_dir)) {
    std::filesystem::create_directories(root);
}

bool LocalDiskStorageEngine::writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data,
                                       bool overwrite) {
    auto full_path = root / rel_path;
    std::filesystem::create_directories(full_path.parent_path());

    if (!overwrite && std::filesystem::exists(full_path)) return false;

    std::ofstream out(full_path, std::ios::binary);
    if (!out) return false;

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return out.good();
}

std::optional<std::vector<uint8_t>> LocalDiskStorageEngine::readFile(const std::filesystem::path& rel_path) const {
    auto full_path = root / rel_path;
    std::ifstream in(full_path, std::ios::binary | std::ios::ate);
    if (!in) return std::nullopt;

    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) return std::nullopt;

    return buffer;
}

bool LocalDiskStorageEngine::deleteFile(const std::filesystem::path& rel_path) {
    auto full_path = root / rel_path;
    return std::filesystem::remove(full_path);
}

bool LocalDiskStorageEngine::fileExists(const std::filesystem::path& rel_path) const {
    return std::filesystem::exists(root / rel_path);
}

std::vector<std::filesystem::path> LocalDiskStorageEngine::listFilesInDir(const std::filesystem::path& rel_path,
                                                                          bool recursive) const {
    std::vector<std::filesystem::path> files;
    auto full_path = root / rel_path;

    if (!std::filesystem::exists(full_path) || !std::filesystem::is_directory(full_path)) return files;

    for (const auto& entry : std::filesystem::directory_iterator(full_path)) {
        if (entry.is_regular_file()) files.push_back(entry.path());
        else if (recursive && entry.is_directory()) {
            auto sub_files = listFilesInDir(entry.path(), true);
            files.insert(files.end(), sub_files.begin(), sub_files.end());
        }
    }

    return files;
}

std::filesystem::path LocalDiskStorageEngine::resolvePath(const std::string& id) const {
    return root / id;
}

std::filesystem::path LocalDiskStorageEngine::getAbsolutePath(const std::filesystem::path& rel_path) const {
    return root / rel_path;
}

fs::path LocalDiskStorageEngine::getRelativePath(const fs::path& absolute_path) const {
    if (absolute_path.is_absolute()) return std::filesystem::relative(absolute_path, root);
    return absolute_path;
}

std::filesystem::path LocalDiskStorageEngine::getRootPath() const {
    return root;
}

} // namespace vh::storage

#include "storage/LocalDiskStorageEngine.hpp"
#include "types/config/ConfigRegistry.hpp"
#include "../../../shared/include/types/Vault.hpp"
#include <fstream>

namespace vh::storage {

LocalDiskStorageEngine::LocalDiskStorageEngine(const std::shared_ptr<types::LocalDiskVault>& vault)
    : StorageEngine(vault, types::config::ConfigRegistry::get().fuse.root_mount_path / vault->mount_point) {
    if (!std::filesystem::exists(root_)) std::filesystem::create_directories(root_);
}

void LocalDiskStorageEngine::mkdir(const std::filesystem::path& relative_path) {
    const auto fullPath = getAbsolutePath(relative_path);
    if (std::filesystem::exists(fullPath)) {
        if (!std::filesystem::is_directory(fullPath)) throw std::runtime_error(
            "Path exists and is not a directory: " + fullPath.string());
        return; // Directory already exists
    }
    std::filesystem::create_directories(fullPath);
}

bool LocalDiskStorageEngine::writeFile(const std::filesystem::path& rel_path,
                                       const std::vector<uint8_t>& data,
                                       const bool overwrite) {
    const auto full_path = root_ / rel_path;
    std::filesystem::create_directories(full_path.parent_path());

    if (!overwrite && std::filesystem::exists(full_path)) return false;

    std::ofstream out(full_path, std::ios::binary);
    if (!out) return false;

    out.write(reinterpret_cast<const char*>(data.data()), data.size());
    return out.good();
}

std::optional<std::vector<uint8_t> > LocalDiskStorageEngine::readFile(const std::filesystem::path& rel_path) const {
    auto full_path = root_ / rel_path;
    std::ifstream in(full_path, std::ios::binary | std::ios::ate);
    if (!in) return std::nullopt;

    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!in.read(reinterpret_cast<char*>(buffer.data()), size)) return std::nullopt;

    return buffer;
}

bool LocalDiskStorageEngine::deleteFile(const std::filesystem::path& rel_path) {
    const auto full_path = root_ / rel_path;
    return std::filesystem::remove(full_path);
}

bool LocalDiskStorageEngine::fileExists(const std::filesystem::path& rel_path) const {
    return std::filesystem::exists(root_ / rel_path);
}

std::filesystem::path LocalDiskStorageEngine::resolvePath(const std::string& id) const {
    return root_ / id;
}

std::filesystem::path LocalDiskStorageEngine::getAbsolutePath(const std::filesystem::path& rel_path) const {
    const auto& fuse_mnt = types::config::ConfigRegistry::get().fuse.root_mount_path;

    if (rel_path.empty()) return fuse_mnt / root_;

    std::filesystem::path safe_rel = rel_path;
    if (safe_rel.is_absolute()) safe_rel = safe_rel.lexically_relative("/");

    return fuse_mnt / root_ / safe_rel;
}

fs::path LocalDiskStorageEngine::getRelativePath(const fs::path& absolute_path) const {
    if (absolute_path.is_absolute()) return std::filesystem::relative(absolute_path, root_);
    return absolute_path;
}

std::filesystem::path LocalDiskStorageEngine::getRootPath() const {
    return root_;
}

} // namespace vh::storage
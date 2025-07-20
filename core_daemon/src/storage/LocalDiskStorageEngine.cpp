#include "storage/LocalDiskStorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/LocalDiskVault.hpp"
#include "util/imageUtil.hpp"
#include "util/files.hpp"
#include "concurrency/thumbnail/ThumbnailWorker.hpp"
#include "database/Queries/FileQueries.hpp"
#include "types/File.hpp"
#include "storage/VaultEncryptionManager.hpp"

#include <fstream>
#include <iostream>

using namespace vh::types;
using namespace vh::database;

namespace vh::storage {

LocalDiskStorageEngine::LocalDiskStorageEngine(const std::shared_ptr<concurrency::ThumbnailWorker>& thumbnailWorker,
                                               const std::shared_ptr<Sync>& sync,
                                               const std::shared_ptr<LocalDiskVault>& vault)
    : StorageEngine(vault, sync, thumbnailWorker, config::ConfigRegistry::get().fuse.root_mount_path / vault->mount_point) {}

void LocalDiskStorageEngine::finishUpload(const unsigned int userId, const std::filesystem::path& relPath) {
    try {
        const auto absPath = getAbsolutePath(relPath);

        if (!std::filesystem::exists(absPath)) throw std::runtime_error("File does not exist at path: " + absPath.string());
        if (!std::filesystem::is_regular_file(absPath)) throw std::runtime_error("Path is not a regular file: " + absPath.string());

        const auto buffer = util::readFileToVector(absPath);

        std::string iv_b64;
        const auto ciphertext = encryptionManager_->encrypt(buffer, iv_b64);

        util::writeFile(absPath, ciphertext);

        const auto f = createFile(relPath, buffer);
        f->created_by = f->last_modified_by = userId;
        f->encryption_iv = iv_b64;

        f->id = FileQueries::upsertFile(f);

        thumbnailWorker_->enqueue(shared_from_this(), buffer, f);

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
}

void LocalDiskStorageEngine::mkdir(const std::filesystem::path& relative_path) {
    const auto fullPath = getAbsolutePath(relative_path);
    if (std::filesystem::exists(fullPath)) {
        if (!std::filesystem::is_directory(fullPath))
            throw std::runtime_error(
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

bool LocalDiskStorageEngine::fileExists(const std::filesystem::path& rel_path) const {
    return std::filesystem::exists(root_ / rel_path);
}

std::filesystem::path LocalDiskStorageEngine::resolvePath(const std::string& id) const {
    return root_ / id;
}

std::filesystem::path LocalDiskStorageEngine::getAbsolutePath(const std::filesystem::path& rel_path) const {
    const auto& fuse_mnt = config::ConfigRegistry::get().fuse.root_mount_path;

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
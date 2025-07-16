#include "storage/StorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/File.hpp"
#include "concurrency/thumbnail/ThumbnailWorker.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "util/Magic.hpp"
#include "crypto/Hash.hpp"

#include <iostream>
#include <algorithm>
#include <fstream>

using namespace vh::types;

namespace vh::storage {

StorageEngine::StorageEngine(const std::shared_ptr<Vault>& vault,
                             const std::shared_ptr<concurrency::ThumbnailWorker>& thumbnailWorker,
                             fs::path root_mount_path)
    : vault_(vault), thumbnailWorker_(thumbnailWorker) {
    const auto conf = config::ConfigRegistry::get();
    cache_path_ = conf.fuse.root_mount_path / conf.caching.path / std::to_string(vault->id);

    if (root_mount_path.empty()) root_ = cache_path_;
    else root_ = std::move(root_mount_path);

    if (!std::filesystem::exists(root_)) {
        std::cout << "[StorageEngine] Creating root directory: " << root_ << std::endl;
        std::filesystem::create_directories(root_);
    } else std::cout << "[StorageEngine] Root directory already exists: " << root_ << std::endl;
}

std::filesystem::path StorageEngine::getRelativePath(const std::filesystem::path& abs_path) const {
    return abs_path.lexically_relative(root_).make_preferred();
}

std::filesystem::path StorageEngine::getAbsolutePath(const std::filesystem::path& rel_path) const {
    if (rel_path.empty()) return root_;

    std::filesystem::path safe_rel = rel_path;
    if (safe_rel.is_absolute()) safe_rel = safe_rel.lexically_relative("/");

    return root_ / safe_rel;
}

std::filesystem::path StorageEngine::getRelativeCachePath(const std::filesystem::path& abs_path) const {
    return abs_path.lexically_relative(cache_path_).make_preferred();
}

std::shared_ptr<File> StorageEngine::createFile(const std::filesystem::path& rel_path, const std::filesystem::path& abs_path) const {
    const auto absPath = abs_path.empty() ? getAbsolutePath(rel_path) : abs_path;

    if (!std::filesystem::exists(absPath)) {
        throw std::runtime_error("File does not exist at path: " + absPath.string());
    }
    if (!std::filesystem::is_regular_file(absPath)) {
        throw std::runtime_error("Path is not a regular file: " + absPath.string());
    }

    auto file = std::make_shared<File>();
    file->vault_id = vault_->id;
    file->name = absPath.filename().string();
    file->size_bytes = std::filesystem::file_size(absPath);
    file->created_by = file->last_modified_by = vault_->owner_id;
    file->path = rel_path;
    file->mime_type = util::Magic::get_mime_type(absPath);
    file->content_hash = crypto::Hash::blake2b(absPath.string());
    const auto parentPath = file->path.has_parent_path() ? std::filesystem::path{"/"} / file->path.parent_path() : std::filesystem::path("/");
    std::cout << "Creating file with parent: { vaultId: " << vault_->id << ", path: " << parentPath.string() << " }" << std::endl;
    file->parent_id = database::DirectoryQueries::getDirectoryIdByPath(vault_->id, parentPath);

    return file;
}

void StorageEngine::writeFile(const std::filesystem::path& abs_path, const std::string& buffer) const {
    if (buffer.empty()) std::cerr << "Warning: empty buffer, writing 0 bytes to: " << abs_path << std::endl;
    if (!std::filesystem::exists(abs_path.parent_path())) std::filesystem::create_directories(abs_path.parent_path());
    std::ofstream file(abs_path, std::ios::binary);
    if (!file) throw std::runtime_error("Failed to open file for writing: " + abs_path.string());
    file.write(buffer.data(), buffer.size());
    file.close();
    std::cout << "[StorageEngine] File written: " << abs_path << std::endl;
}

std::filesystem::path StorageEngine::getAbsoluteCachePath(const std::filesystem::path& rel_path,
                                                          const std::filesystem::path& prefix) const {
    const auto relPath = rel_path.string().starts_with("/") ? fs::path(rel_path.string().substr(1)) : rel_path;
    if (prefix.empty()) return cache_path_ / relPath;

    const auto prefixPath = prefix.string().starts_with("/") ? fs::path(prefix.string().substr(1)) : prefix;
    return cache_path_ / prefixPath / relPath;
}

uintmax_t StorageEngine::getDirectorySize(const std::filesystem::path& path) {
    uintmax_t total = 0;
    for (auto& p : std::filesystem::recursive_directory_iterator(path, std::filesystem::directory_options::skip_permission_denied))
        if (std::filesystem::is_regular_file(p.status())) total += std::filesystem::file_size(p);
    return total;
}

unsigned int StorageEngine::vaultId() const { return vault_ ? vault_->id : 0; }

uintmax_t StorageEngine::getVaultSize() const { return getDirectorySize(root_); }
uintmax_t StorageEngine::getCacheSize() const { return getDirectorySize(cache_path_); }
uintmax_t StorageEngine::getVaultAndCacheTotalSize() const { return getVaultSize() + getCacheSize(); }

uintmax_t StorageEngine::freeSpace() const {
    return vault_->quota - getVaultAndCacheTotalSize() - MIN_FREE_SPACE;
}

void StorageEngine::purgeThumbnails(const fs::path& rel_path) const {
    for (const auto& size : config::ConfigRegistry::get().caching.thumbnails.sizes) {
        const auto thumbnailPath = getAbsoluteCachePath(rel_path, fs::path("thumbnails") / std::to_string(size));
        if (std::filesystem::exists(thumbnailPath)) std::filesystem::remove(thumbnailPath);
    }
}

std::string StorageEngine::getMimeType(const std::filesystem::path& path) {
    static const std::unordered_map<std::string, std::string> mimeMap = {
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".png", "image/png"},
        {".pdf", "application/pdf"}, {".txt", "text/plain"}, {".html", "text/html"},
    };

    std::string ext = path.extension().string();
    std::ranges::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    const auto it = mimeMap.find(ext);
    return it != mimeMap.end() ? it->second : "application/octet-stream";
}

}
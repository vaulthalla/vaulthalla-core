#include "storage/StorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Operation.hpp"
#include "concurrency/thumbnail/ThumbnailWorker.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/OperationQueries.hpp"
#include "util/Magic.hpp"
#include "crypto/Hash.hpp"
#include "storage/VaultEncryptionManager.hpp"

#include <iostream>
#include <algorithm>
#include <fstream>
#include <utility>

using namespace vh::types;
using namespace vh::database;

namespace vh::storage {

StorageEngine::StorageEngine(const std::shared_ptr<Vault>& vault,
                             const std::shared_ptr<Sync>& sync,
                             const std::shared_ptr<concurrency::ThumbnailWorker>& thumbnailWorker,
                             fs::path root_mount_path)
    : sync_(sync), root_(std::move(root_mount_path)), vault_(vault), thumbnailWorker_(thumbnailWorker) {
    const auto& conf = config::ConfigRegistry::get();
    cache_path_ = conf.fuse.root_mount_path / conf.caching.path / std::to_string(vault->id);
    if (!std::filesystem::exists(root_)) std::filesystem::create_directories(root_);
    if (!std::filesystem::exists(cache_path_)) std::filesystem::create_directories(cache_path_);
    encryptionManager_ = std::make_shared<VaultEncryptionManager>(root_);
}

bool StorageEngine::isDirectory(const std::filesystem::path& rel_path) const {
    return DirectoryQueries::isDirectory(vault_->id, rel_path);
}

bool StorageEngine::isFile(const std::filesystem::path& rel_path) const {
    return FileQueries::isFile(vault_->id, rel_path);
}

void StorageEngine::move(const std::filesystem::path& from, const std::filesystem::path& to, const unsigned int userId) const {
    if (from == to) return;

    const bool isFile = this->isFile(from);

    if (!isFile && !isDirectory(from)) throw std::runtime_error("[StorageEngine] Path does not exist: " + from.string());

    std::shared_ptr<FSEntry> entry;
    if (isFile) entry = FileQueries::getFileByPath(vaultId(), from);
    else entry = DirectoryQueries::getDirectoryByPath(vaultId(), from);

    if (isFile) FileQueries::moveFile(std::static_pointer_cast<File>(entry), to, userId);
    else DirectoryQueries::moveDirectory(std::static_pointer_cast<Directory>(entry), to, userId);

    OperationQueries::addOperation(std::make_shared<Operation>(entry, to, userId, Operation::Op::Move));
}

void StorageEngine::rename(const std::filesystem::path& from, const std::filesystem::path& to, const unsigned int userId) const {
    if (from == to) return;

    const bool isFile = this->isFile(from);

    if (!isFile && !isDirectory(from)) throw std::runtime_error("[StorageEngine] Path does not exist: " + from.string());

    std::shared_ptr<FSEntry> entry;
    if (isFile) entry = FileQueries::getFileByPath(vaultId(), from);
    else entry = DirectoryQueries::getDirectoryByPath(vaultId(), from);

    entry->name = to.filename().string();
    entry->path = to;
    entry->last_modified_by = userId;

    if (isFile) FileQueries::upsertFile(std::static_pointer_cast<File>(entry));
    else DirectoryQueries::upsertDirectory(std::static_pointer_cast<Directory>(entry));

    OperationQueries::addOperation(std::make_shared<Operation>(entry, to, userId, Operation::Op::Rename));
}

void StorageEngine::copy(const std::filesystem::path& from, const std::filesystem::path& to, const unsigned int userId) const {
    if (from == to) return;

    const bool isFile = this->isFile(from);

    if (!isFile && !isDirectory(from)) throw std::runtime_error("[StorageEngine] Path does not exist: " + from.string());

    std::shared_ptr<FSEntry> entry;
    if (isFile) entry = FileQueries::getFileByPath(vaultId(), from);
    else entry = DirectoryQueries::getDirectoryByPath(vaultId(), from);

    entry->id = 0;
    entry->path = to;
    entry->name = to.filename().string();
    entry->created_at = {};
    entry->updated_at = {};
    entry->created_by = entry->last_modified_by = userId;

    if (isFile) {
        auto newFile = std::make_shared<File>(*std::static_pointer_cast<File>(entry));
        newFile->id = FileQueries::upsertFile(newFile);
        OperationQueries::addOperation(std::make_shared<Operation>(newFile, to, userId, Operation::Op::Copy));
    } else {
        auto newDir = std::make_shared<Directory>(*std::static_pointer_cast<Directory>(entry));
        DirectoryQueries::upsertDirectory(newDir);
        OperationQueries::addOperation(std::make_shared<Operation>(newDir, to, userId, Operation::Op::Copy));
    }
}

void StorageEngine::remove(const std::filesystem::path& rel_path, const unsigned int userId) const {
    if (isDirectory(rel_path)) removeDirectory(rel_path, userId);
    else if (isFile(rel_path)) removeFile(rel_path, userId);
    else throw std::runtime_error("[StorageEngine] Path does not exist: " + rel_path.string());
}

void StorageEngine::removeFile(const fs::path& rel_path, const unsigned int userId) const {
    FileQueries::markFileAsTrashed(userId, vault_->id, rel_path);
}

void StorageEngine::removeDirectory(const fs::path& rel_path, const unsigned int userId) const {
    for (const auto& file : FileQueries::listFilesInDir(vault_->id, rel_path, true))
        FileQueries::markFileAsTrashed(userId, file->id);
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

std::shared_ptr<File> StorageEngine::createFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& buffer) const {
    const auto absPath = getAbsolutePath(rel_path);

    if (!std::filesystem::exists(absPath))
        throw std::runtime_error("File does not exist at path: " + absPath.string());
    if (!std::filesystem::is_regular_file(absPath))
        throw std::runtime_error("Path is not a regular file: " + absPath.string());

    auto file = std::make_shared<File>();
    file->vault_id = vault_->id;
    file->name = absPath.filename().string();
    file->size_bytes = std::filesystem::file_size(absPath);
    file->created_by = file->last_modified_by = vault_->owner_id;
    file->path = rel_path;
    file->mime_type = buffer.empty() ? util::Magic::get_mime_type(absPath) : util::Magic::get_mime_type_from_buffer(buffer);
    file->content_hash = crypto::Hash::blake2b(absPath.string());
    const auto parentPath = file->path.has_parent_path() ? std::filesystem::path{"/"} / file->path.parent_path() : std::filesystem::path("/");
    file->parent_id = DirectoryQueries::getDirectoryIdByPath(vault_->id, parentPath);

    return file;
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

std::vector<uint8_t> StorageEngine::decrypt(const unsigned int vaultId, const std::filesystem::path& relPath, const std::vector<uint8_t>& payload) const {
    const auto iv = FileQueries::getEncryptionIV(vaultId, relPath);
    if (iv.empty()) throw std::runtime_error("No encryption IV found for file: " + relPath.string());
    return encryptionManager_->decrypt(payload, iv);
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
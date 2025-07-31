#include "storage/StorageEngine.hpp"
#include "config/ConfigRegistry.hpp"
#include "types/Vault.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Operation.hpp"
#include "types/Path.hpp"
#include "util/Magic.hpp"
#include "crypto/Hash.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/SyncQueries.hpp"
#include "database/Queries/OperationQueries.hpp"
#include "storage/VaultEncryptionManager.hpp"
#include "storage/Filesystem.hpp"
#include "util/fsPath.hpp"
#include "util/files.hpp"
#include "services/ThumbnailWorker.hpp"


#include <iostream>

using namespace vh::types;
using namespace vh::database;
using namespace vh::encryption;
using namespace vh::config;
using namespace vh::storage;
using namespace vh::services;
namespace fs = std::filesystem;

namespace vh::storage {

StorageEngine::StorageEngine(const std::shared_ptr<Vault>& vault)
    : vault(vault),
      sync(SyncQueries::getSync(vault->id)),
      paths(std::make_shared<Path>(vault->mount_point)),
      encryptionManager(std::make_shared<VaultEncryptionManager>(paths->vaultRoot)) {
    if (!FSEntryQueries::exists(paths->vaultRoot)) Filesystem::mkVault(paths->absRelToRoot(paths->vaultRoot, PathType::FUSE_ROOT), vault->id);
    if (!fs::exists(paths->cacheRoot)) fs::create_directories(paths->cacheRoot);
}

bool StorageEngine::isDirectory(const fs::path& rel_path) const {
    return DirectoryQueries::isDirectory(vault->id, rel_path);
}

bool StorageEngine::isFile(const fs::path& rel_path) const {
    return FileQueries::isFile(vault->id, rel_path);
}

std::shared_ptr<File> StorageEngine::createFile(const fs::path& rel_path, const std::vector<uint8_t>& buffer) const {
    const auto absPath = paths->absPath(rel_path, PathType::BACKING_VAULT_ROOT);

    if (!fs::exists(absPath))
        throw std::runtime_error("File does not exist at path: " + absPath.string());
    if (!fs::is_regular_file(absPath))
        throw std::runtime_error("Path is not a regular file: " + absPath.string());

    auto file = std::make_shared<File>();
    file->vault_id = vault->id;
    file->name = absPath.filename().string();
    file->size_bytes = fs::file_size(absPath);
    file->created_by = file->last_modified_by = vault->owner_id;
    file->path = rel_path;
    file->abs_path = makeAbsolute(absPath);
    file->mime_type = buffer.empty() ? util::Magic::get_mime_type(absPath) : util::Magic::get_mime_type_from_buffer(buffer);
    file->content_hash = crypto::Hash::blake2b(absPath.string());
    const auto parentPath = file->path.has_parent_path() ? fs::path{"/"} / file->path.parent_path() : fs::path("/");
    file->parent_id = DirectoryQueries::getDirectoryIdByPath(vault->id, parentPath);

    return file;
}

std::vector<uint8_t> StorageEngine::decrypt(const unsigned int vaultId, const fs::path& relPath, const std::vector<uint8_t>& payload) const {
    const auto iv = FileQueries::getEncryptionIV(vaultId, relPath);
    if (iv.empty()) throw std::runtime_error("No encryption IV found for file: " + relPath.string());
    return encryptionManager->decrypt(payload, iv);
}

uintmax_t StorageEngine::getDirectorySize(const fs::path& path) {
    uintmax_t total = 0;
    for (auto& p : fs::recursive_directory_iterator(path, fs::directory_options::skip_permission_denied))
        if (fs::is_regular_file(p.status())) total += fs::file_size(p);
    return total;
}

uintmax_t StorageEngine::getVaultSize() const { return getDirectorySize(paths->backingRoot); }
uintmax_t StorageEngine::getCacheSize() const { return getDirectorySize(paths->cacheRoot); }
uintmax_t StorageEngine::getVaultAndCacheTotalSize() const { return getVaultSize() + getCacheSize(); }

uintmax_t StorageEngine::freeSpace() const {
    return vault->quota - getVaultAndCacheTotalSize() - MIN_FREE_SPACE;
}

void StorageEngine::purgeThumbnails(const fs::path& rel_path) const {
    for (const auto& size : ConfigRegistry::get().caching.thumbnails.sizes) {
        const auto thumbnailPath = paths->absPath(rel_path, PathType::THUMBNAIL_ROOT) / std::to_string(size);
        if (fs::exists(thumbnailPath)) fs::remove(thumbnailPath);
    }
}

std::string StorageEngine::getMimeType(const fs::path& path) {
    static const std::unordered_map<std::string, std::string> mimeMap = {
        {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"}, {".png", "image/png"},
        {".pdf", "application/pdf"}, {".txt", "text/plain"}, {".html", "text/html"},
    };

    std::string ext = path.extension().string();
    std::ranges::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    const auto it = mimeMap.find(ext);
    return it != mimeMap.end() ? it->second : "application/octet-stream";
}

void StorageEngine::moveThumbnails(const std::filesystem::path& from, const std::filesystem::path& to) const {
    for (const auto& size : ConfigRegistry::get().caching.thumbnails.sizes) {
        auto fromPath = paths->absPath(from, PathType::THUMBNAIL_ROOT) / std::to_string(size);
        auto toPath = paths->absPath(to, PathType::THUMBNAIL_ROOT) / std::to_string(size);

        if (fromPath.extension() != ".jpg" && fromPath.extension() != ".jpeg") {
            fromPath += ".jpg";
            toPath += ".jpg";
        }

        if (!fs::exists(fromPath)) {
            std::cerr << "[StorageEngine] Thumbnail does not exist: " << fromPath.string() << std::endl;
            continue;
        }

        Filesystem::mkdir(toPath.parent_path());
        fs::rename(fromPath, toPath); // TODO: Handle rename properly
    }
}

void StorageEngine::copyThumbnails(const std::filesystem::path& from, const std::filesystem::path& to) const {
    for (const auto& size : ConfigRegistry::get().caching.thumbnails.sizes) {
        auto fromPath = paths->absPath(from, PathType::THUMBNAIL_ROOT) / std::to_string(size);
        auto toPath = paths->absPath(to, PathType::THUMBNAIL_ROOT) / std::to_string(size);

        if (fromPath.extension() != ".jpg" && fromPath.extension() != ".jpeg") {
            fromPath += ".jpg";
            toPath += ".jpg";
        }

        if (!fs::exists(fromPath)) {
            std::cerr << "[StorageEngine] Thumbnail does not exist: " << fromPath.string() << std::endl;
            continue;
        }

        Filesystem::mkdir(toPath.parent_path());
        fs::copy_file(fromPath, toPath, fs::copy_options::overwrite_existing);  // TODO: Handle copy properly
    }
}

void StorageEngine::finishUpload(const unsigned int userId, const std::filesystem::path& relPath) {
    const auto absPath = paths->absPath(relPath, PathType::BACKING_VAULT_ROOT);

    if (!std::filesystem::exists(absPath)) throw std::runtime_error("File does not exist at path: " + absPath.string());
    if (!std::filesystem::is_regular_file(absPath)) throw std::runtime_error("Path is not a regular file: " + absPath.string());

    const auto buffer = util::readFileToVector(absPath);

    std::string iv_b64;
    const auto ciphertext = encryptionManager->encrypt(buffer, iv_b64);
    util::writeFile(absPath, ciphertext);

    const auto f = createFile(relPath);
    f->created_by = f->last_modified_by = userId;
    f->encryption_iv = iv_b64;
    f->id = FileQueries::upsertFile(f);

    ThumbnailWorker::enqueue(shared_from_this(), buffer, f);
}

void StorageEngine::mkdir(const fs::path& relPath, const unsigned int userId) const {
    const auto absPath = paths->absPath(relPath, PathType::BACKING_VAULT_ROOT);
    if (!fs::exists(absPath)) fs::create_directories(absPath);

    const auto d = std::make_shared<Directory>();

    d->vault_id = vault->id;
    d->name = fs::path(relPath).filename().string();
    d->created_by = d->last_modified_by = userId;
    d->path = relPath;
    d->abs_path = absPath;
    d->parent_id = DirectoryQueries::getDirectoryIdByPath(vault->id, fs::path(relPath).parent_path());

    DirectoryQueries::upsertDirectory(d);
}

void StorageEngine::move(const fs::path& from, const fs::path& to, const unsigned int userId) const {
    if (from == to) return;

    const bool isFile = this->isFile(from);

    if (!isFile && !isDirectory(from)) throw std::runtime_error("[StorageEngine] Path does not exist: " + from.string());

    std::shared_ptr<FSEntry> entry;
    if (isFile) entry = FileQueries::getFileByPath(vault->id, from);
    else entry = DirectoryQueries::getDirectoryByPath(vault->id, from);

    if (isFile) FileQueries::moveFile(std::static_pointer_cast<File>(entry), to, userId);
    else DirectoryQueries::moveDirectory(std::static_pointer_cast<Directory>(entry), to, userId);

    OperationQueries::addOperation(std::make_shared<Operation>(entry, to, userId, Operation::Op::Move));
}

void StorageEngine::rename(const fs::path& from, const fs::path& to, const unsigned int userId) const {
    if (from == to) return;

    const bool isFile = this->isFile(from);

    if (!isFile && !isDirectory(from)) throw std::runtime_error("[StorageEngine] Path does not exist: " + from.string());

    std::shared_ptr<FSEntry> entry;
    if (isFile) entry = FileQueries::getFileByPath(vault->id, from);
    else entry = DirectoryQueries::getDirectoryByPath(vault->id, from);

    OperationQueries::addOperation(std::make_shared<Operation>(entry, to, userId, Operation::Op::Rename));

    entry->name = to.filename().string();
    entry->path = to;
    entry->abs_path = paths->absPath(to, PathType::VAULT_ROOT);
    entry->last_modified_by = userId;

    if (isFile) FileQueries::upsertFile(std::static_pointer_cast<File>(entry));
    else DirectoryQueries::upsertDirectory(std::static_pointer_cast<Directory>(entry));
}

void StorageEngine::copy(const fs::path& from, const fs::path& to, const unsigned int userId) const {
    if (from == to) return;

    const bool isFile = this->isFile(from);

    if (!isFile && !isDirectory(from)) throw std::runtime_error("[StorageEngine] Path does not exist: " + from.string());

    std::shared_ptr<FSEntry> entry;
    if (isFile) entry = FileQueries::getFileByPath(vault->id, from);
    else entry = DirectoryQueries::getDirectoryByPath(vault->id, from);

    OperationQueries::addOperation(std::make_shared<Operation>(entry, to, userId, Operation::Op::Copy));

    entry->id = 0;
    entry->path = to;
    entry->abs_path = paths->absPath(to, PathType::VAULT_ROOT);
    entry->name = to.filename().string();
    entry->created_at = {};
    entry->updated_at = {};
    entry->created_by = entry->last_modified_by = userId;
    entry->parent_id = DirectoryQueries::getDirectoryIdByPath(vault->id, to.parent_path());

    if (isFile) FileQueries::upsertFile(std::make_shared<File>(*std::static_pointer_cast<File>(entry)));
    else DirectoryQueries::upsertDirectory(std::make_shared<Directory>(*std::static_pointer_cast<Directory>(entry)));
}

void StorageEngine::remove(const fs::path& rel_path, const unsigned int userId) const {
    if (isDirectory(rel_path)) removeDirectory(rel_path, userId);
    else if (isFile(rel_path)) removeFile(rel_path, userId);
    else throw std::runtime_error("[StorageEngine] Path does not exist: " + rel_path.string());
}

void StorageEngine::removeFile(const fs::path& rel_path, const unsigned int userId) const {
    FileQueries::markFileAsTrashed(userId, vault->id, rel_path);
}

void StorageEngine::removeDirectory(const fs::path& rel_path, const unsigned int userId) const {
    for (const auto& file : FileQueries::listFilesInDir(vault->id, rel_path, true))
        FileQueries::markFileAsTrashed(userId, file->id);
}

}
#include "storage/CloudStorageEngine.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/S3Vault.hpp"
#include "types/Sync.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "concurrency/thumbnail/ThumbnailWorker.hpp"
#include "protocols/websocket/handlers/UploadHandler.hpp"
#include "storage/VaultEncryptionManager.hpp"
#include "util/files.hpp"

#include <iostream>
#include <fstream>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

CloudStorageEngine::CloudStorageEngine(const std::shared_ptr<concurrency::ThumbnailWorker>& thumbnailWorker,
                                       const std::shared_ptr<S3Vault>& vault,
                                       const std::shared_ptr<api::APIKey>& key,
                                       const std::shared_ptr<Sync>& sync)
    : StorageEngine(vault, sync, thumbnailWorker,
        config::ConfigRegistry::get().fuse.root_mount_path / config::ConfigRegistry::get().caching.path / std::to_string(vault->id) / "files"),
      key_(key) {
    const auto s3Key = std::static_pointer_cast<api::S3APIKey>(key_);
    s3Provider_ = std::make_shared<cloud::S3Provider>(s3Key, vault->bucket);
}

void CloudStorageEngine::finishUpload(const unsigned int userId, const std::filesystem::path& relPath) {
    const auto absPath = getAbsolutePath(relPath);

    if (!std::filesystem::exists(absPath)) throw std::runtime_error("File does not exist at path: " + absPath.string());
    if (!std::filesystem::is_regular_file(absPath)) throw std::runtime_error("Path is not a regular file: " + absPath.string());

    const auto buffer = util::readFileToVector(absPath);

    std::string iv_b64;
    const auto ciphertext = encryptionManager_->encrypt(buffer, iv_b64);
    util::writeFile(absPath, ciphertext);

    const auto f = createFile(relPath);
    f->created_by = f->last_modified_by = userId;
    f->encryption_iv = iv_b64;
    f->id = FileQueries::upsertFile(f);

    thumbnailWorker_->enqueue(shared_from_this(), buffer, f);
}

void CloudStorageEngine::mkdir(const std::filesystem::path& relative_path) {
    std::cout << "[CloudStorageEngine] mkdir called: " << relative_path << std::endl;
}

std::optional<std::vector<uint8_t> > CloudStorageEngine::readFile(const std::filesystem::path& rel_path) const {
    std::cout << "[CloudStorageEngine] readFile called: " << rel_path << std::endl;
    // TODO: Implement S3/R2 read logic
    return std::nullopt;
}

void CloudStorageEngine::purge(const std::filesystem::path& rel_path) const {
    removeLocally(rel_path);
    removeRemotely(rel_path, false);
}

void CloudStorageEngine::removeLocally(const std::filesystem::path& rel_path) const {
    const auto path = rel_path.string().front() != '/' ? std::filesystem::path("/" / rel_path) : rel_path;
    purgeThumbnails(path);
    FileQueries::deleteFile(vault_->owner_id, vault_->id, path);
    const auto absPath = getAbsolutePath(path);
    if (std::filesystem::exists(absPath)) std::filesystem::remove(absPath);
}

void CloudStorageEngine::removeRemotely(const std::filesystem::path& rel_path, const bool rmThumbnails) const {
    if (!s3Provider_->deleteObject(stripLeadingSlash(rel_path))) throw std::runtime_error(
        "[CloudStorageEngine] Failed to delete object from S3: " + rel_path.string());
    if (rmThumbnails) purgeThumbnails(rel_path);
}

bool CloudStorageEngine::fileExists(const std::filesystem::path& rel_path) const {
    std::cout << "[CloudStorageEngine] fileExists called: " << rel_path << std::endl;
    // TODO: Implement S3/R2 fileExists logic
    return false;
}

void CloudStorageEngine::uploadFile(const std::filesystem::path& rel_path) const {
    const auto absPath = getAbsolutePath(rel_path);
    if (!std::filesystem::exists(absPath) || !std::filesystem::is_regular_file(absPath))
        throw std::runtime_error("[CloudStorageEngine] Invalid file: " + absPath.string());

    const std::filesystem::path s3Key = stripLeadingSlash(rel_path);

    if (std::filesystem::file_size(absPath) < 5 * 1024 * 1024) s3Provider_->uploadObject(stripLeadingSlash(rel_path), absPath);
    else s3Provider_->uploadLargeObject(stripLeadingSlash(rel_path), absPath);

    std::vector<uint8_t> buffer;
    if (!s3Provider_->downloadToBuffer(s3Key, buffer))
        throw std::runtime_error("[CloudStorageEngine] Failed to download uploaded file: " + s3Key.string());

    s3Provider_->setObjectContentHash(s3Key, FileQueries::getContentHash(vaultId(), rel_path));
    s3Provider_->setObjectEncryptionMetadata(s3Key, FileQueries::getEncryptionIV(vaultId(), rel_path));
}

std::vector<uint8_t> CloudStorageEngine::downloadToBuffer(const std::filesystem::path& rel_path) const {
    std::vector<uint8_t> buffer;
    if (!s3Provider_->downloadToBuffer(stripLeadingSlash(rel_path), buffer))
        throw std::runtime_error("[CloudStorageEngine] Failed to download file: " + rel_path.string());

    return buffer;
}

std::shared_ptr<File> CloudStorageEngine::downloadFile(const std::filesystem::path& rel_path) {
    auto buffer = downloadToBuffer(rel_path);
    const auto absPath = getAbsolutePath(rel_path);
    const std::filesystem::path s3Key = stripLeadingSlash(rel_path);

    std::shared_ptr<File> file;

    if (remoteFileIsEncrypted(rel_path)) {
        auto iv_b64 = getRemoteIVBase64(rel_path);
        if (!iv_b64) {
            try {
                iv_b64 = FileQueries::getEncryptionIV(vault_->id, rel_path);
            } catch (const std::exception& e) {
                std::cerr << "[CloudStorageEngine] Failed to get IV for encrypted file: " << e.what() << std::endl;
                throw std::runtime_error("[CloudStorageEngine] No IV found for encrypted file: " + rel_path.string());
            }
        }
        if (iv_b64) {
            const auto decrypted = encryptionManager_->decrypt(buffer, *iv_b64);
            file = createFile(rel_path, decrypted);
            file->encryption_iv = *iv_b64;
        }
        s3Provider_->setObjectEncryptionMetadata(s3Key, *iv_b64);
        if (file->content_hash) s3Provider_->setObjectContentHash(s3Key, *file->content_hash);
        util::writeFile(absPath, buffer);
    }
    else {
        std::string iv_b64;
        const auto ciphertext = encryptionManager_->encrypt(buffer, iv_b64);
        util::writeFile(absPath, ciphertext);

        file = createFile(rel_path, buffer);
        file->encryption_iv = iv_b64;

        std::unordered_map<std::string, std::string> meta{
        {"vh-encrypted", "true"},
        {"content-hash", *file->content_hash}
        };
        if (!s3Provider_->uploadBufferWithMetadata(s3Key, buffer, meta))
            throw std::runtime_error("[CloudStorageEngine] Failed to upload file with metadata: " + rel_path.string());
        buffer = downloadToBuffer(rel_path);
    }

    file->id = FileQueries::upsertFile(file);

    thumbnailWorker_->enqueue(shared_from_this(), buffer, file);

    return file;
}

void CloudStorageEngine::indexAndDeleteFile(const std::filesystem::path& rel_path) {
    const auto index = downloadFile(rel_path);
    std::filesystem::remove(getAbsoluteCachePath(index->path, {"files"}));
}

std::unordered_map<std::u8string, std::shared_ptr<File> > CloudStorageEngine::getGroupedFilesFromS3(const std::filesystem::path& prefix) const {
    return groupEntriesByPath(filesFromS3XML(s3Provider_->listObjects(prefix)));
}

std::string CloudStorageEngine::getRemoteContentHash(const std::filesystem::path& rel_path) const {
    if (const auto head = s3Provider_->getHeadObject(stripLeadingSlash(rel_path)))
        if (head->contains("x-amz-meta-content-hash")) return head->at("x-amz-meta-content-hash");
    return "";
}

bool CloudStorageEngine::remoteFileIsEncrypted(const std::filesystem::path& rel_path) const {
    const auto s3Key = stripLeadingSlash(rel_path);

    if (const auto head = s3Provider_->getHeadObject(s3Key)) {
        const auto it = head->find("x-amz-meta-vh-encrypted");
        if (it != head->end()) {
            const std::string& val = it->second;
            return val == "true" || val == "1";
        }
    }

    return false; // assume unencrypted if metadata is missing or head request failed
}

std::vector<std::shared_ptr<Directory>> CloudStorageEngine::extractDirectories(
    const std::vector<std::shared_ptr<File>>& files) const {

    std::unordered_map<std::u8string, std::shared_ptr<Directory>> directories;

    for (const auto& file : files) {
        std::filesystem::path current = "/";
        std::filesystem::path full_path = file->path.parent_path();

        for (const auto& part : full_path) {
            current /= part;
            auto dir_str = current.u8string();

            if (!directories.contains(dir_str)) {
                auto dir = std::make_shared<Directory>();
                dir->path = current;
                dir->name = current.filename().string();
                dir->created_by = dir->last_modified_by = vault_->owner_id;
                dir->vault_id = vault_->id;
                const auto parent_path = current.has_parent_path() ? current.parent_path() : "/";
                dir->parent_id = DirectoryQueries::getDirectoryIdByPath(vault_->id, parent_path);

                directories[dir_str] = dir;
            }
        }
    }

    std::vector<std::shared_ptr<Directory>> result;
    for (const auto& [_, dir] : directories)
        result.push_back(dir);

    std::ranges::sort(result, [](const auto& a, const auto& b) {
        return std::distance(a->path.begin(), a->path.end()) < std::distance(b->path.begin(), b->path.end());
    });

    return result;
}

std::optional<std::string> CloudStorageEngine::getRemoteIVBase64(const std::filesystem::path& rel_path) const {
    const std::filesystem::path s3Key = stripLeadingSlash(rel_path);

    if (const auto head = s3Provider_->getHeadObject(s3Key)) {
        const auto it = head->find("x-amz-meta-vh-iv");
        if (it != head->end()) return it->second;
    }

    return std::nullopt;
}

std::u8string vh::storage::stripLeadingSlash(const std::filesystem::path& path) {
    std::u8string u8 = path.u8string();
    if (!u8.empty() && u8[0] == u8'/') return u8.substr(1);
    return u8;
}

#include "storage/CloudStorageEngine.hpp"
#include "keys/VaultEncryptionManager.hpp"
#include "storage/cloud/S3Provider.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/S3Vault.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "util/files.hpp"
#include "util/fsPath.hpp"
#include "services/ThumbnailWorker.hpp"
#include "storage/Filesystem.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"

#include <fstream>

using namespace vh::storage;
using namespace vh::types;
using namespace vh::keys;
using namespace vh::concurrency;
using namespace vh::services;

CloudStorageEngine::CloudStorageEngine(const std::shared_ptr<S3Vault>& vault)
    : StorageEngine(vault),
      key_(ServiceDepsRegistry::instance().apiKeyManager->getAPIKey(vault->api_key_id, vault->owner_id)),
      s3Provider_(std::make_shared<cloud::S3Provider>(key_, vault->bucket)) {}

void CloudStorageEngine::purge(const std::filesystem::path& rel_path) const {
    removeLocally(rel_path);
    removeRemotely(rel_path, false);
}

void CloudStorageEngine::removeLocally(const std::filesystem::path& rel_path) const {
    const auto path = rel_path.string().front() != '/' ? std::filesystem::path("/" / rel_path) : rel_path;
    purgeThumbnails(path);
    const auto file = FileQueries::getFileByPath(vault->id, path);
    FileQueries::deleteFile(vault->owner_id, file);
    const auto absPath = paths->absPath(path, PathType::BACKING_VAULT_ROOT);
    if (std::filesystem::exists(absPath)) std::filesystem::remove(absPath);
}

void CloudStorageEngine::removeRemotely(const std::filesystem::path& rel_path, const bool rmThumbnails) const {
    if (!s3Provider_->deleteObject(stripLeadingSlash(rel_path))) throw std::runtime_error(
        "[CloudStorageEngine] Failed to delete object from S3: " + rel_path.string());
    if (rmThumbnails) purgeThumbnails(rel_path);
}

void CloudStorageEngine::uploadFile(const std::filesystem::path& rel_path) const {
    const auto absPath = paths->absPath(rel_path, PathType::BACKING_VAULT_ROOT);
    if (!std::filesystem::exists(absPath) || !std::filesystem::is_regular_file(absPath))
        throw std::runtime_error("[CloudStorageEngine] Invalid file: " + absPath.string());

    const std::filesystem::path s3Key = stripLeadingSlash(rel_path);

    if (std::filesystem::file_size(absPath) < 5 * 1024 * 1024)
        s3Provider_->uploadObject(stripLeadingSlash(rel_path), absPath);
    else s3Provider_->uploadLargeObject(stripLeadingSlash(rel_path), absPath);

    std::vector<uint8_t> buffer;
    if (!s3Provider_->downloadToBuffer(s3Key, buffer))
        throw std::runtime_error("[CloudStorageEngine] Failed to download uploaded file: " + s3Key.string());

    s3Provider_->setObjectContentHash(s3Key, FileQueries::getContentHash(vault->id, rel_path));

    if (const auto iv = FileQueries::getEncryptionIV(vault->id, rel_path))
        s3Provider_->setObjectEncryptionMetadata(s3Key, *iv);
}

std::vector<uint8_t> CloudStorageEngine::downloadToBuffer(const std::filesystem::path& rel_path) const {
    std::vector<uint8_t> buffer;
    if (!s3Provider_->downloadToBuffer(stripLeadingSlash(rel_path), buffer))
        throw std::runtime_error("[CloudStorageEngine] Failed to download file: " + rel_path.string());

    return buffer;
}

std::shared_ptr<File> CloudStorageEngine::downloadFile(const std::filesystem::path& rel_path) {
    auto buffer = downloadToBuffer(rel_path);
    const auto absPath = paths->absPath(rel_path, PathType::BACKING_VAULT_ROOT);
    const std::filesystem::path s3Key = stripLeadingSlash(rel_path);

    std::shared_ptr<File> file;

    if (remoteFileIsEncrypted(rel_path)) {
        auto iv_b64 = getRemoteIVBase64(rel_path);
        if (!iv_b64) iv_b64 = FileQueries::getEncryptionIV(vault->id, rel_path);
        if (!iv_b64) throw std::runtime_error("[CloudStorageEngine] No IV found for encrypted file: " + rel_path.string());
        buffer = encryptionManager->decrypt(buffer, *iv_b64);
    }

    Filesystem::createFile({
            .path = makeAbsolute(rel_path),
            .fuse_path = paths->absRelToAbsRel(absPath, PathType::VAULT_ROOT, PathType::FUSE_ROOT),
            .buffer = buffer,
            .engine = shared_from_this(),
            .userId = vault->owner_id
        });

    std::unordered_map<std::string, std::string> meta{
            {"vh-encrypted", "true"},
            {"content-hash", *file->content_hash}
    };

    if (!s3Provider_->uploadBufferWithMetadata(s3Key, buffer, meta))
        throw std::runtime_error("[CloudStorageEngine] Failed to reupload new file with enhanced metadata: " + rel_path.string());

    ThumbnailWorker::enqueue(shared_from_this(), buffer, file);

    return file;
}

void CloudStorageEngine::indexAndDeleteFile(const std::filesystem::path& rel_path) {
    const auto index = downloadFile(rel_path);
    std::filesystem::remove(paths->absPath(index->path, PathType::FILE_CACHE_ROOT));
}

std::unordered_map<std::u8string, std::shared_ptr<File>>
CloudStorageEngine::getGroupedFilesFromS3(const std::filesystem::path& prefix) const {
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
                dir->created_by = dir->last_modified_by = vault->owner_id;
                dir->vault_id = vault->id;
                const auto parent_path = current.has_parent_path() ? current.parent_path() : "/";
                dir->parent_id = DirectoryQueries::getDirectoryIdByPath(vault->id, parent_path);

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

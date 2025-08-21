#include "storage/CloudStorageEngine.hpp"
#include "crypto/VaultEncryptionManager.hpp"
#include "storage/cloud/S3Controller.hpp"
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

using namespace vh::storage;
using namespace vh::types;
using namespace vh::crypto;
using namespace vh::concurrency;
using namespace vh::services;

CloudStorageEngine::CloudStorageEngine(const std::shared_ptr<S3Vault>& vault)
    : StorageEngine(vault),
      key_(ServiceDepsRegistry::instance().apiKeyManager->getAPIKey(vault->api_key_id, vault->owner_id)),
      s3Provider_(std::make_shared<cloud::S3Controller>(key_, vault->bucket)) {}

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

void CloudStorageEngine::uploadFile(const std::shared_ptr<File>& f) const {
    if (!std::filesystem::exists(f->backing_path) || !std::filesystem::is_regular_file(f->backing_path))
        throw std::runtime_error("[CloudStorageEngine] Invalid file: " + f->path.string());

    const std::filesystem::path s3Key = stripLeadingSlash(f->path);

    if (std::filesystem::file_size(f->backing_path) < 5 * 1024 * 1024)
        s3Provider_->uploadObject(s3Key, f->backing_path);
    else s3Provider_->uploadLargeObject(s3Key, f->backing_path);

    std::vector<uint8_t> buffer;
    if (!s3Provider_->downloadToBuffer(s3Key, buffer))
        throw std::runtime_error("[CloudStorageEngine] Failed to download uploaded file: " + s3Key.string());

    s3Provider_->setObjectContentHash(s3Key, f->content_hash ? *f->content_hash : FileQueries::getContentHash(vault->id, f->path));

    if (!s3Provider_->setObjectEncryptionMetadata(s3Key, f->encryption_iv, f->encrypted_with_key_version))
        throw std::runtime_error("[CloudStorageEngine] Failed to set encryption metadata for file: " + f->path.string());
}

void CloudStorageEngine::uploadFileBuffer(const std::shared_ptr<File>& f, const std::vector<uint8_t>& buffer) const {
    if (buffer.empty())
        throw std::invalid_argument("[CloudStorageEngine] Buffer for upload cannot be empty");

    if (!f || f->path.empty())
        throw std::invalid_argument("[CloudStorageEngine] Invalid file or buffer for upload");

    const auto s3Key = stripLeadingSlash(f->path);
    const std::unordered_map<std::string, std::string> meta{
        {"vh-encrypted", f->encryption_iv == "" ? "false" : "true"},
        {"vh-key-version", std::to_string(f->encrypted_with_key_version)},
        {"vh-iv", f->encryption_iv},
        {"content-hash", *f->content_hash}
    };

    if (!s3Provider_->uploadBufferWithMetadata(s3Key, buffer, meta))
        throw std::runtime_error("[CloudStorageEngine] Failed to upload file buffer: " + f->path.string());
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
        auto payload = getRemoteIVBase64AndVersion(rel_path);
        if (!payload) payload = FileQueries::getEncryptionIVAndVersion(vault->id, rel_path);
        if (!payload) throw std::runtime_error("[CloudStorageEngine] No IV found for encrypted file: " + rel_path.string());
        const auto& [iv_b64, key_version] = *payload;
        buffer = encryptionManager->decrypt(buffer, iv_b64, key_version);;
    }

    Filesystem::createFile({
            .path = makeAbsolute(rel_path),
            .fuse_path = paths->absRelToAbsRel(absPath, PathType::VAULT_ROOT, PathType::FUSE_ROOT),
            .buffer = buffer,
            .engine = shared_from_this(),
            .userId = vault->owner_id
        });

    const std::unordered_map<std::string, std::string> meta{
            {"vh-encrypted", "true"},
            {"vh-key-version", std::to_string(file->encrypted_with_key_version)},
            {"vh-iv", file->encryption_iv},
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

std::optional<std::pair<std::string, unsigned int>> CloudStorageEngine::getRemoteIVBase64AndVersion(const std::filesystem::path& rel_path) const {
    const std::filesystem::path s3Key = stripLeadingSlash(rel_path);

    std::string iv_b64;
    unsigned int key_version = 0;

    if (const auto head = s3Provider_->getHeadObject(s3Key)) {
        const auto it = head->find("x-amz-meta-vh-iv");
        if (it != head->end()) iv_b64 = it->second;
        const auto versionIt = head->find("x-amz-meta-vh-key-version");
        if (versionIt != head->end()) key_version = std::stoul(versionIt->second);
    }

    if (iv_b64.empty() || key_version == 0) {
        LogRegistry::cloud()->error("[CloudStorageEngine] No IV or key version found for encrypted file: {}", rel_path.string());
        return std::nullopt;
    }

    return std::make_pair(iv_b64, key_version);
}

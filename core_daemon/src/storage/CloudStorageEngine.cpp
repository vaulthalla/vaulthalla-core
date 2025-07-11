#include "storage/CloudStorageEngine.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Vault.hpp"
#include "database/Queries/FileQueries.hpp"
#include "util/imageUtil.hpp"
#include "config/ConfigRegistry.hpp"

#include <iostream>
#include <fstream>
#include <ranges>

namespace vh::storage {

CloudStorageEngine::CloudStorageEngine(const std::shared_ptr<types::S3Vault>& vault,
                                       const std::shared_ptr<types::api::APIKey>& key)
                                           : StorageEngine(vault),
                                             key_(key) {
    const auto conf = config::ConfigRegistry::get();
    cache_expiry_days_ = conf.cloud.cache.expiry_days;
    if (!std::filesystem::exists(root_)) std::filesystem::create_directories(root_);
    if (!std::filesystem::exists(cache_path_)) std::filesystem::create_directories(cache_path_);

    const auto s3Key = std::static_pointer_cast<types::api::S3APIKey>(key_);
    s3Provider_ = std::make_shared<cloud::S3Provider>(s3Key, vault->bucket);
}

void CloudStorageEngine::finishUpload(const std::filesystem::path& rel_path, const std::string& mime_type) {
    const auto absPath = getAbsolutePath(rel_path);
    if (!std::filesystem::exists(absPath) || !std::filesystem::is_regular_file(absPath)) {
        throw std::runtime_error("[CloudStorageEngine] Invalid file: " + absPath.string());
    }

    const auto s3Key = rel_path.string().starts_with("/") ? rel_path.string().substr(1) : rel_path.string();

    if (std::filesystem::file_size(absPath) < 5 * 1024 * 1024) s3Provider_->uploadObject(s3Key, absPath.string());
    else s3Provider_->uploadLargeObject(s3Key, absPath.string());

    std::string buffer;
    if (!s3Provider_->downloadToBuffer(s3Key, buffer))
        throw std::runtime_error("[CloudStorageEngine] Failed to download uploaded file: " + s3Key);

    auto thumbnailPath = getAbsoluteCachePath(rel_path);
    try {
        util::generateAndStoreThumbnail(buffer, thumbnailPath, mime_type);
    } catch (const std::exception& e) {
        s3Provider_->deleteObject(rel_path);
        std::cerr << "[CloudStorageEngine] Thumbnail gen failed, deleted: " << rel_path << std::endl;
    }
}

void CloudStorageEngine::mkdir(const std::filesystem::path& relative_path) {
    std::cout << "[CloudStorageEngine] mkdir called: " << relative_path << std::endl;
}

std::optional<std::vector<uint8_t> > CloudStorageEngine::readFile(const std::filesystem::path& rel_path) const {
    std::cout << "[CloudStorageEngine] readFile called: " << rel_path << std::endl;
    // TODO: Implement S3/R2 read logic
    return std::nullopt;
}

void CloudStorageEngine::deleteFile(const std::filesystem::path& rel_path) {
    if (!std::filesystem::remove(getAbsoluteCachePath(rel_path)))
        throw std::runtime_error("[CloudStorageEngine] Failed to delete cache file: " + rel_path.string());

    if (!s3Provider_->deleteObject(rel_path))
        throw std::runtime_error("[CloudStorageEngine] Failed to delete object from S3: " + rel_path.string());
}

bool CloudStorageEngine::fileExists(const std::filesystem::path& rel_path) const {
    std::cout << "[CloudStorageEngine] fileExists called: " << rel_path << std::endl;
    // TODO: Implement S3/R2 fileExists logic
    return false;
}

bool CloudStorageEngine::isDirectory(const std::filesystem::path& rel_path) const {
    return database::FileQueries::isDirectory(vault_->id, rel_path);
}

bool CloudStorageEngine::isFile(const std::filesystem::path& rel_path) const {
    return database::FileQueries::isFile(vault_->id, rel_path);
}

std::filesystem::path CloudStorageEngine::getAbsolutePath(const std::filesystem::path& rel_path) const {
    if (rel_path.empty()) return root_;

    std::filesystem::path safe_rel = rel_path;
    if (safe_rel.is_absolute()) safe_rel = safe_rel.lexically_relative("/");

    return root_ / safe_rel;
}


void CloudStorageEngine::initCloudStorage() const {
    std::cout << "[CloudStorageEngine] Initializing cloud storage for vault: " << vault_->id << std::endl;
    const auto s3Vault = std::static_pointer_cast<types::S3Vault>(vault_);

    std::string buffer;
    for (auto& item : types::fromS3XML(s3Provider_->listObjects())) {
        buffer.clear(); // Clear buffer for each item
        item->vault_id = vault_->id;
        item->created_by = vault_->owner_id;
        item->last_modified_by = vault_->owner_id;

        if (item->isDirectory()) {
            if (item->path.string() == "/") continue;
            auto dir = std::static_pointer_cast<types::Directory>(item);
            dir->parent_id = database::FileQueries::getDirectoryIdByPath(vault_->id, dir->path.parent_path());
            database::FileQueries::addDirectory(dir);
        } else {
            auto file = std::static_pointer_cast<types::File>(item);
            const auto parentPath = file->path.parent_path().empty() ? std::filesystem::path("/") : file->path.parent_path();
            file->parent_id = database::FileQueries::getDirectoryIdByPath(vault_->id, parentPath);
            file->mime_type = getMimeType(file->path);
            database::FileQueries::addFile(file);

            if (file->mime_type->starts_with("image/") || file->mime_type->starts_with("application/pdf")) {
                const auto s3Path = getAbsoluteCachePath(file->path).string();
                std::filesystem::path cachePath = root_ / s3Path;
                cachePath.replace_extension(".jpg");
                try {
                    util::generateAndStoreThumbnail(s3Path, cachePath, *file->mime_type);
                } catch (const std::exception& e) {
                    std::cerr << "[CloudStorageEngine] Thumbnail generation failed for " << file->path
                              << ": " << e.what() << std::endl;
                    s3Provider_->deleteObject(file->path.string());  // Clean up if thumbnail generation fails
                }
            }
        }
    }
}

std::string getMimeType(const std::filesystem::path& path) {
    static const std::unordered_map<std::string, std::string> mimeMap = {
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".png", "image/png"},
        {".pdf", "application/pdf"},
        {".txt", "text/plain"},
        {".html", "text/html"},
    };

    std::string ext = path.extension().string();
    std::ranges::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    const auto it = mimeMap.find(ext);
    return it != mimeMap.end() ? it->second : "application/octet-stream";
}

}
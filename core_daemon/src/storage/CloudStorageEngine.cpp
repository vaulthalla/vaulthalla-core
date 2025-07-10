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
    root_ = conf.fuse.root_mount_path / conf.cloud.cache.cache_path / std::to_string(vault->id);
    if (!std::filesystem::exists(root_)) {
        std::cout << "[CloudStorageEngine] Creating cache directory: " << root_ << std::endl;
        std::filesystem::create_directories(root_);
    } else {
        std::cout << "[CloudStorageEngine] Cache directory already exists: " << root_ << std::endl;
    }

    const auto s3Key = std::static_pointer_cast<types::api::S3APIKey>(key_);
    s3Provider_ = std::make_shared<cloud::S3Provider>(s3Key, vault->bucket);
}

void CloudStorageEngine::mkdir(const std::filesystem::path& relative_path) {
    std::cout << "[CloudStorageEngine] mkdir called: " << relative_path << std::endl;
}

std::optional<std::vector<uint8_t> > CloudStorageEngine::readFile(const std::filesystem::path& rel_path) const {
    std::cout << "[CloudStorageEngine] readFile called: " << rel_path << std::endl;
    // TODO: Implement S3/R2 read logic
    return std::nullopt;
}

bool CloudStorageEngine::deleteFile(const std::filesystem::path& rel_path) {
    std::cout << "[CloudStorageEngine] deleteFile called: " << rel_path << std::endl;
    // TODO: Implement S3/R2 delete logic
    return true;
}

bool CloudStorageEngine::fileExists(const std::filesystem::path& rel_path) const {
    std::cout << "[CloudStorageEngine] fileExists called: " << rel_path << std::endl;
    // TODO: Implement S3/R2 fileExists logic
    return false;
}

void CloudStorageEngine::uploadFile(const std::filesystem::path& rel_path, bool overwrite) {
    const auto absPath = getAbsolutePath(rel_path);
    if (!std::filesystem::exists(absPath) || !std::filesystem::is_regular_file(absPath)) {
        throw std::runtime_error("[CloudStorageEngine] Invalid file: " + absPath.string());
    }

    const auto s3Key = rel_path.string().starts_with("/") ? rel_path.string().substr(1) : rel_path.string();

    if (std::filesystem::file_size(absPath) < 5 * 1024 * 1024)
        s3Provider_->uploadObject(s3Key, absPath.string());
    else
        s3Provider_->uploadLargeObject(s3Key, absPath.string());

    std::string buffer;
    if (!s3Provider_->downloadToBuffer(s3Key, buffer)) {
        throw std::runtime_error("[CloudStorageEngine] Failed to download uploaded file: " + s3Key);
    }

    const auto mime = getMimeType(rel_path);
    const bool isImage = mime.starts_with("image/");
    const bool isPdf = mime == "application/pdf";

    if (!isImage && !isPdf) {
        s3Provider_->deleteObject(s3Key);
        std::cerr << "[CloudStorageEngine] Unsupported file type, deleted from remote: " << s3Key << std::endl;
        return;
    }

    std::vector<uint8_t> jpeg;
    if (isImage) {
        std::vector<uint8_t> raw(buffer.begin(), buffer.end());
        jpeg = util::resize_and_compress_image_buffer(raw.data(), raw.size(), std::nullopt, std::make_optional("128"));
    } else if (isPdf) {
        jpeg = util::resize_and_compress_pdf_buffer(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size(), std::nullopt, std::make_optional("128"));
    }

    if (jpeg.empty()) {
        s3Provider_->deleteObject(s3Key);
        std::cerr << "[CloudStorageEngine] Failed to generate thumbnail, deleted: " << s3Key << std::endl;
        return;
    }

    const std::filesystem::path thumbnailPath = absPath.parent_path() / rel_path.filename().replace_extension(".jpg");
    std::ofstream outFile(thumbnailPath, std::ios::binary);
    outFile.write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
    outFile.close();

    std::cout << "[CloudStorageEngine] Successfully uploaded and cached thumbnail: " << thumbnailPath << std::endl;
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
            auto dir = std::static_pointer_cast<types::Directory>(item);
            dir->parent_id = database::FileQueries::getDirectoryIdByPath(vault_->id, dir->path.parent_path());
            database::FileQueries::addDirectory(dir);
        } else {
            auto file = std::static_pointer_cast<types::File>(item);
            const auto parentPath = file->path.parent_path().empty() ? std::filesystem::path("/") : file->path.parent_path();
            file->parent_id = database::FileQueries::getDirectoryIdByPath(vault_->id, parentPath);
            file->mime_type = getMimeType(file->path);
            database::FileQueries::addFile(file);

            // Process image or PDF thumbnails
            const bool isImage = file->mime_type->starts_with("image/");
            const bool isPdf   = file->mime_type->starts_with("application/pdf");

            if (isImage || isPdf) {
                try {
                    const auto s3Path = file->path.string().starts_with("/") ? file->path.string().substr(1) : file->path.string();
                    if (!s3Provider_->downloadToBuffer(s3Path, buffer)) {
                        std::cerr << "[CloudStorageEngine] Failed to download file: " << s3Path << std::endl;
                        continue;
                    }

                    std::vector<uint8_t> jpeg;
                    if (isImage) {
                        std::vector<uint8_t> raw(buffer.begin(), buffer.end());
                        jpeg = util::resize_and_compress_image_buffer(
                            raw.data(),
                            raw.size(),
                            std::nullopt,
                            std::make_optional("128")
                        );
                    } else if (isPdf) {
                        jpeg = util::resize_and_compress_pdf_buffer(
                            reinterpret_cast<const uint8_t*>(buffer.data()),
                            buffer.size(),
                            std::nullopt,
                            std::make_optional("128")
                        );
                    }

                    if (!jpeg.empty()) {
                        std::filesystem::path localPath = root_ / s3Path;
                        localPath.replace_extension(".jpg");
                        std::filesystem::create_directories(localPath.parent_path());
                        try {
                            std::ofstream outFile(localPath, std::ios::binary);
                            outFile.write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
                            outFile.close();
                            std::cout << "[CloudStorageEngine] Successfully resized and cached: " << file->path << std::endl;
                        } catch (const std::exception& e) {
                            std::cerr << "[CloudStorageEngine] Error writing file: " << e.what() << std::endl;
                            throw;
                        }
                    } else {
                        std::cerr << "[CloudStorageEngine] Failed to resize file: " << file->path << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[CloudStorageEngine] Error processing file: " << file->path << " - " << e.what() << std::endl;
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
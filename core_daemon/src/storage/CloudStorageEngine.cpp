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

CloudStorageEngine::CloudStorageEngine(const std::shared_ptr<types::Vault>& vault,
                                       const std::shared_ptr<types::api::APIKey>& key)
                                           : StorageEngine(vault),
                                             key_(key),
                                             s3Provider_(std::make_shared<cloud::S3Provider>(std::static_pointer_cast<types::api::S3APIKey>(key_))) {
    const auto conf = config::ConfigRegistry::get();
    cache_expiry_days_ = conf.cloud.cache.expiry_days;
    root_ = conf.fuse.root_mount_path / conf.cloud.cache.cache_path / std::to_string(vault->id);
}

void CloudStorageEngine::mkdir(const std::filesystem::path& relative_path) {
    std::cout << "[CloudStorageEngine] mkdir called: " << relative_path << std::endl;
}

bool CloudStorageEngine::writeFile(const std::filesystem::path& rel_path, const std::vector<uint8_t>& data,
                                   bool overwrite) {
    std::cout << "[CloudStorageEngine] writeFile called: " << rel_path << std::endl;
    // TODO: Implement S3/R2 write logic
    return true;
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

std::filesystem::path CloudStorageEngine::getAbsolutePath(const std::filesystem::path& rel_path) const {
    if (rel_path.empty()) return root_;

    std::filesystem::path safe_rel = rel_path;
    if (safe_rel.is_absolute()) safe_rel = safe_rel.lexically_relative("/");

    return root_ / safe_rel;
}


void CloudStorageEngine::initCloudStorage() const {
    std::cout << "[CloudStorageEngine] Initializing cloud storage for vault: " << vault_->id << std::endl;

    const auto s3Vault = std::static_pointer_cast<types::S3Vault>(vault_);

    types::Directory rootDir;
    rootDir.vault_id = vault_->id;
    rootDir.name = "/";
    rootDir.created_by = vault_->owner_id;
    rootDir.last_modified_by = vault_->owner_id;
    rootDir.path = "/";
    rootDir.stats = std::make_shared<types::DirectoryStats>();
    rootDir.stats->size_bytes = 0;
    rootDir.stats->file_count = 0;
    rootDir.stats->subdirectory_count = 0;

    const auto rootId = database::FileQueries::addDirectory(rootDir);

    std::string buffer;
    for (auto& item : types::fromS3XML(s3Provider_->listObjects(s3Vault->bucket))) {
        item->vault_id = vault_->id;
        item->created_by = vault_->owner_id;
        item->last_modified_by = vault_->owner_id;

        if (item->isDirectory()) {
            auto dir = std::static_pointer_cast<types::Directory>(item);
            dir->parent_id = database::FileQueries::getDirectoryIdByPath(vault_->id, dir->path.parent_path());
            database::FileQueries::addDirectory(dir);
        } else {
            auto file = std::static_pointer_cast<types::File>(item);
            file->parent_id = database::FileQueries::getDirectoryIdByPath(vault_->id, file->path.parent_path());
            file->mime_type = getMimeType(file->path);
            const auto fileId = database::FileQueries::addFile(file);

            if (file->mime_type->starts_with("image/") || file->mime_type->starts_with("application")) {
                const auto success = s3Provider_->downloadToBuffer(s3Vault->bucket, file->path.string(), buffer);
                if (success) {
                    const auto jpeg = util::resize_and_compress_image(buffer, std::nullopt, std::make_optional("128"));
                    if (!jpeg.empty()) {
                        std::filesystem::path localPath = root_ / file->path;
                        std::filesystem::create_directories(localPath.parent_path());
                        std::ofstream outFile(localPath, std::ios::binary);
                        try {
                            outFile.write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
                            outFile.close();
                        } catch (const std::exception& e) {
                            std::cerr << "[CloudStorageEngine] Error writing resized image to file: " << e.what() << std::endl;
                            continue;
                        }

                        std::cout << "[CloudStorageEngine] Successfully downloaded and resized file: " << file->path << std::endl;
                    } else std::cerr << "[CloudStorageEngine] Failed to resize image: " << file->path << std::endl;
                } else std::cerr << "[CloudStorageEngine] Failed to download file: " << file->path << std::endl;
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
        // Add more as needed
    };

    std::string ext = path.extension().string();
    std::ranges::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    const auto it = mimeMap.find(ext);
    return it != mimeMap.end() ? it->second : "application/octet-stream";
}

} // namespace vh::storage
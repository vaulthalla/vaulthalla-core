#include "storage/CloudStorageEngine.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Vault.hpp"
#include "database/Queries/FileQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "services/ThumbnailWorker.hpp"

#include <iostream>
#include <fstream>
#include <pugixml.hpp>

#include "protocols/websocket/handlers/UploadHandler.hpp"

namespace vh::storage {

CloudStorageEngine::CloudStorageEngine(const std::shared_ptr<services::ThumbnailWorker>& thumbnailWorker,
                                       const std::shared_ptr<types::S3Vault>& vault,
                                       const std::shared_ptr<types::api::APIKey>& key)
    : StorageEngine(vault, thumbnailWorker),
      key_(key) {
    const auto conf = config::ConfigRegistry::get();
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

    thumbnailWorker_->enqueue(shared_from_this(), buffer, rel_path, mime_type);
}

void CloudStorageEngine::mkdir(const std::filesystem::path& relative_path) {
    std::cout << "[CloudStorageEngine] mkdir called: " << relative_path << std::endl;
}

std::optional<std::vector<uint8_t> > CloudStorageEngine::readFile(const std::filesystem::path& rel_path) const {
    std::cout << "[CloudStorageEngine] readFile called: " << rel_path << std::endl;
    // TODO: Implement S3/R2 read logic
    return std::nullopt;
}

void CloudStorageEngine::remove(const std::filesystem::path& rel_path) {
    std::cout << "[CloudStorageEngine] remove called for: " << rel_path << std::endl;
    if (isDirectory(rel_path)) removeDirectory(rel_path);
    else if (isFile(rel_path)) removeFile(rel_path);
    else throw std::runtime_error("[CloudStorageEngine] Path does not exist: " + rel_path.string());
}

void CloudStorageEngine::removeFile(const fs::path& rel_path) {
    std::cout << "[CloudStorageEngine] Deleting file: " << rel_path << "\n";

    if (!s3Provider_->deleteObject(stripLeadingSlash(rel_path))) throw std::runtime_error(
        "[CloudStorageEngine] Failed to delete object from S3: " + rel_path.string());

    purgeThumbnails(rel_path);
    database::FileQueries::deleteFile(vault_->id, rel_path);
}

void CloudStorageEngine::removeDirectory(const fs::path& rel_path) {
    std::cout << "[CloudStorageEngine] remove called for directory: " << rel_path << std::endl;

    const auto files = database::FileQueries::listFilesInDir(vault_->id, rel_path, true);
    const auto directories = database::FileQueries::listDirectoriesInDir(vault_->id, rel_path, true);

    for (const auto& file : files) removeFile(file->path);

    std::cout << "[CloudStorageEngine] Deleting directories in: " << rel_path << "\n";
    for (const auto& dir : directories)
        database::FileQueries::deleteDirectory(vault_->id, dir->path);

    std::cout << "[CloudStorageEngine] Deleting directory: " << rel_path << "\n";
    database::FileQueries::deleteDirectory(vault_->id, rel_path);

    std::cout << "[CloudStorageEngine] Directory removed: " << rel_path << std::endl;
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


void CloudStorageEngine::initCloudStorage() {
    std::cout << "[CloudStorageEngine] Initializing cloud storage for vault: " << vault_->id << std::endl;
    const auto s3Vault = std::static_pointer_cast<types::S3Vault>(vault_);

    std::string buffer;
    for (auto& item : types::fromS3XML(s3Provider_->listObjects())) {
        buffer.clear(); // Clear buffer for each item
        item->vault_id = vault_->id;
        item->created_by = vault_->owner_id;
        item->last_modified_by = vault_->owner_id;

        const auto parentPath = item->path.parent_path().empty()
                                        ? std::filesystem::path("/")
                                        : item->path.parent_path();

        if (item->isDirectory()) {
            if (item->path.string() == "/") continue;
            auto dir = std::static_pointer_cast<types::Directory>(item);
            dir->parent_id = database::FileQueries::getDirectoryIdByPath(vault_->id, parentPath);
            database::FileQueries::addDirectory(dir);
        } else {
            auto file = std::static_pointer_cast<types::File>(item);
            file->parent_id = database::FileQueries::getDirectoryIdByPath(vault_->id, parentPath);
            file->mime_type = getMimeType(file->path);
            database::FileQueries::addFile(file);

            if (file->mime_type->starts_with("image/") || file->mime_type->starts_with("application/pdf"))
                thumbnailWorker_->enqueue(shared_from_this(), buffer, file->path, *file->mime_type);
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

std::vector<std::string> s3KeysFromXML(const std::string& xml, const std::filesystem::path& rel_path) {
    std::vector<std::string> keys;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml.c_str());

    if (!result) {
        std::cerr << "[s3KeysFromXML] Failed to parse XML: " << result.description() << std::endl;
        return {};
    }

    pugi::xml_node root = doc.child("ListBucketResult");
    if (!root) {
        std::cerr << "[s3KeysFromXML] Missing <ListBucketResult> root node" << std::endl;
        return {};
    }

    const std::string prefix = rel_path.string().empty() ? "" : rel_path.string().append(rel_path.string().back() == '/' ? "" : "/");

    for (pugi::xml_node content : root.children("Contents")) {
        auto keyNode = content.child("Key");
        if (!keyNode) continue;

        std::string key = keyNode.text().get();
        if (key.starts_with(prefix)) keys.push_back(std::move(key));
    }

    return keys;
}

std::u8string stripLeadingSlash(const std::filesystem::path& path) {
    std::u8string u8 = path.u8string();
    if (!u8.empty() && u8[0] == u8'/') return u8.substr(1);
    return u8;
}

}
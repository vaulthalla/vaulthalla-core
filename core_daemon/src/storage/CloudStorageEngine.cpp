#include "storage/CloudStorageEngine.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/S3Vault.hpp"
#include "types/Sync.hpp"
#include "types/CacheIndex.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "services/ThumbnailWorker.hpp"

#include <iostream>
#include <fstream>
#include <pugixml.hpp>

#include "protocols/websocket/handlers/UploadHandler.hpp"

using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

CloudStorageEngine::CloudStorageEngine(const std::shared_ptr<services::ThumbnailWorker>& thumbnailWorker,
                                       const std::shared_ptr<S3Vault>& vault,
                                       const std::shared_ptr<api::APIKey>& key,
                                       const std::shared_ptr<Sync>& sync)
    : StorageEngine(vault, thumbnailWorker),
      key_(key), sync(sync) {
    const auto conf = config::ConfigRegistry::get();
    if (!std::filesystem::exists(root_)) std::filesystem::create_directories(root_);
    if (!std::filesystem::exists(cache_path_)) std::filesystem::create_directories(cache_path_);

    const auto s3Key = std::static_pointer_cast<api::S3APIKey>(key_);
    s3Provider_ = std::make_shared<cloud::S3Provider>(s3Key, vault->bucket);
}

void CloudStorageEngine::finishUpload(const unsigned int userId, const std::filesystem::path& relPath) {
    const auto absPath = getAbsolutePath(relPath);

    if (!std::filesystem::exists(absPath)) throw std::runtime_error("File does not exist at path: " + absPath.string());
    if (!std::filesystem::is_regular_file(absPath)) throw std::runtime_error(
        "Path is not a regular file: " + absPath.string());

    const auto f = createFile(relPath);
    f->created_by = f->last_modified_by = userId;

    std::string buffer;
    uploadFile(f->path, buffer);
    thumbnailWorker_->enqueue(shared_from_this(), buffer, f);

    if (!FileQueries::fileExists(vault_->id, f->path)) FileQueries::addFile(f);
    else FileQueries::updateFile(f);
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
    FileQueries::deleteFile(vault_->id, rel_path);
}

void CloudStorageEngine::removeDirectory(const fs::path& rel_path) {
    std::cout << "[CloudStorageEngine] remove called for directory: " << rel_path << std::endl;

    const auto files = FileQueries::listFilesInDir(vault_->id, rel_path, true);
    const auto directories = DirectoryQueries::listDirectoriesInDir(vault_->id, rel_path, true);

    for (const auto& file : files) removeFile(file->path);

    std::cout << "[CloudStorageEngine] Deleting directories in: " << rel_path << "\n";
    for (const auto& dir : directories)
        DirectoryQueries::deleteDirectory(vault_->id, dir->path);

    std::cout << "[CloudStorageEngine] Deleting directory: " << rel_path << "\n";
    DirectoryQueries::deleteDirectory(vault_->id, rel_path);

    std::cout << "[CloudStorageEngine] Directory removed: " << rel_path << std::endl;
}

bool CloudStorageEngine::fileExists(const std::filesystem::path& rel_path) const {
    std::cout << "[CloudStorageEngine] fileExists called: " << rel_path << std::endl;
    // TODO: Implement S3/R2 fileExists logic
    return false;
}

bool CloudStorageEngine::isDirectory(const std::filesystem::path& rel_path) const {
    return DirectoryQueries::isDirectory(vault_->id, rel_path);
}

bool CloudStorageEngine::isFile(const std::filesystem::path& rel_path) const {
    return FileQueries::isFile(vault_->id, rel_path);
}

void CloudStorageEngine::uploadFile(const std::filesystem::path& rel_path) const {
    std::string buffer;
    uploadFile(rel_path, buffer);
}

void CloudStorageEngine::uploadFile(const std::filesystem::path& rel_path, std::string& buffer) const {
    const auto absPath = getAbsolutePath(rel_path);
    if (!std::filesystem::exists(absPath) || !std::filesystem::is_regular_file(absPath))
        throw std::runtime_error("[CloudStorageEngine] Invalid file: " + absPath.string());

    const auto s3Key = rel_path.string().starts_with("/") ? rel_path.string().substr(1) : rel_path.string();

    if (std::filesystem::file_size(absPath) < 5 * 1024 * 1024) s3Provider_->uploadObject(stripLeadingSlash(rel_path), absPath);
    else s3Provider_->uploadLargeObject(stripLeadingSlash(rel_path), absPath);

    if (!s3Provider_->downloadToBuffer(s3Key, buffer))
        throw std::runtime_error("[CloudStorageEngine] Failed to download uploaded file: " + s3Key);
}

std::string CloudStorageEngine::downloadToBuffer(const std::filesystem::path& rel_path) const {
    std::string buffer;
    if (!s3Provider_->downloadToBuffer(rel_path, buffer))
        throw std::runtime_error("[CloudStorageEngine] Failed to download file: " + rel_path.string());

    return buffer;
}

std::shared_ptr<File> CloudStorageEngine::downloadFile(const std::filesystem::path& rel_path) {
    const auto buffer = downloadToBuffer(rel_path);
    const auto absPath = getAbsolutePath(rel_path);
    writeFile(absPath, buffer);
    const auto file = createFile(absPath);
    thumbnailWorker_->enqueue(shared_from_this(), buffer, file);

    if (!FileQueries::fileExists(vault_->id, file->path)) FileQueries::addFile(file);
    else FileQueries::updateFile(file);

    s3Provider_->setObjectContentHash(file->path, file->content_hash);

    return file;
}

std::shared_ptr<CacheIndex> CloudStorageEngine::cacheFile(const std::filesystem::path& rel_path) {
    const auto buffer = downloadToBuffer(rel_path);
    const auto cacheAbsPath = getAbsoluteCachePath(rel_path, {"files"});
    writeFile(cacheAbsPath, buffer);
    const auto file = createFile(cacheAbsPath);
    thumbnailWorker_->enqueue(shared_from_this(), buffer, file);

    if (!FileQueries::fileExists(vault_->id, file->path)) FileQueries::addFile(file);
    else FileQueries::updateFile(file);

    s3Provider_->setObjectContentHash(file->path, file->content_hash);

    const auto cacheIndex = std::make_shared<CacheIndex>();
    cacheIndex->vault_id = vault_->id;
    cacheIndex->path = getRelativeCachePath(cacheAbsPath);
    cacheIndex->file_id = file->id;
    cacheIndex->type = CacheIndex::Type::File;
    cacheIndex->size = std::filesystem::file_size(cacheAbsPath);

    CacheQueries::upsertCacheIndex(cacheIndex);

    return CacheQueries::getCacheIndexByPath(vault_->id, cacheIndex->path);
}

void CloudStorageEngine::indexAndDeleteFile(const std::filesystem::path& rel_path) {
    const auto file = downloadFile(rel_path);
    thumbnailWorker_->enqueue(shared_from_this(), file->path, file);

    if (!FileQueries::fileExists(vault_->id, file->path)) FileQueries::addFile(file);
    else FileQueries::updateFile(file);

    s3Provider_->setObjectContentHash(file->path, file->content_hash);
    removeFile(rel_path);
}

std::unordered_map<std::u8string, std::shared_ptr<File> > CloudStorageEngine::getGroupedFilesFromS3(const std::filesystem::path& prefix) const {
    return groupEntriesByPath(filesFromS3XML(s3Provider_->listObjects(prefix)));
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

std::u8string stripLeadingSlash(const std::filesystem::path& path) {
    std::u8string u8 = path.u8string();
    if (!u8.empty() && u8[0] == u8'/') return u8.substr(1);
    return u8;
}

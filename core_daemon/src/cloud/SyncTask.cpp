#include "cloud/SyncTask.hpp"
#include "storage/CloudStorageEngine.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Vault.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/SyncQueries.hpp"
#include "crypto/Hash.hpp"
#include "util/Magic.hpp"
#include "types/ProxySync.hpp"
#include "services/SyncController.hpp"
#include "storage/StorageManager.hpp"
#include "services/ThumbnailWorker.hpp"

#include <iostream>
#include <unordered_map>

using namespace vh::cloud;

SyncTask::SyncTask(const std::shared_ptr<storage::CloudStorageEngine>& engine,
                   const std::shared_ptr<services::SyncController>& syncController)
    : next_run(std::chrono::system_clock::from_time_t(engine->proxySync_->last_sync_at)
             + std::chrono::seconds(engine->proxySync_->interval.count())),
      engine_(engine), controller_(syncController) {}

void SyncTask::operator()() {
    if (!engine_) {
        std::cerr << "[SyncWorker] Engine is not initialized!" << std::endl;
        return;
    }

    bool taskIsActive;

    {
        std::shared_lock lock(controller_->engineMapMutex_);
        taskIsActive = engine_->proxySync_->enabled && controller_->engineMap_.contains(engine_->vault_->id);
    }

    if (!taskIsActive) {
        std::cout << "[SyncWorker] Engine not found in controller map for vault ID: " << engine_->vault_->id << "\n"
                  << "             Killing task for vault: " << engine_->vault_->name << std::endl;
        return;
    }

    if (!database::FileQueries::directoryExists(engine_->getVault()->id, "/")) {
        std::cout << "[SyncWorker] Root directory does not exist, creating: " << engine_->vault_->name << std::endl;
        const auto dir = std::make_shared<types::Directory>();
        dir->vault_id = engine_->vault_->id;
        dir->name = "/";
        dir->created_by = dir->last_modified_by = engine_->vault_->owner_id;
        dir->path = "/";
        dir->parent_id = std::nullopt; // Root directory has no parent
        database::FileQueries::addDirectory(dir);
    }

    std::cout << "[SyncWorker] Starting sync for vault: " << engine_->vault_->name << std::endl;
    database::SyncQueries::reportSyncStarted(engine_->proxySync_->id);

    auto s3Map = types::groupEntriesByPath(types::filesFromS3XML(engine_->s3Provider_->listObjects()));

    sync(s3Map);
    cacheDiff(s3Map);

    // TODO: Download diff if vault configured
    // downloadDiff(s3Map);

    std::cout << "[SyncWorker] Sync completed successfully for vault: " << engine_->vault_->name << std::endl;

    database::SyncQueries::reportSyncSuccess(engine_->proxySync_->id);

    next_run = std::chrono::system_clock::now() + std::chrono::seconds(engine_->proxySync_->interval.count());
    controller_->requeue(std::const_pointer_cast<SyncTask>(shared_from_this()));
}

void SyncTask::sync(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const {
    for (const auto& file : database::FileQueries::listFilesInDir(engine_->getVault()->id, "/")) {
        const auto s3Match = s3Map.find(file->path.u8string());
        if (s3Match != s3Map.end()) {
            const auto s3Obj = s3Match->second;
            const auto s3Meta = engine_->s3Provider_->getHeadObject(s3Obj->path);

            if (s3Meta && s3Meta->contains(CONTENT_HASH_ID) && s3Meta->at(CONTENT_HASH_ID) == file->content_hash) {
                std::cout << "[SyncWorker] Content hash matches, skipping: " << file->path << "\n";
                continue;
            }

            if (s3Obj->updated_at == file->updated_at) {
                std::cout << "[SyncWorker] Resolving content hash mismatch: " << file->path << "\n";

                if (file->size_bytes == s3Obj->size_bytes) {
                    std::cout << "[SyncWorker] Sizes match, updating content hash: " << file->path << "\n";
                    engine_->s3Provider_->setObjectContentHash(file->path, file->content_hash);
                } else {
                    std::cout << "[SyncWorker] Sizes differ, re-uploading: " << file->path << "\n";
                    engine_->uploadFile(file->path);
                }
            } else if (s3Obj->updated_at < file->updated_at) {
                std::cout << "[SyncWorker] Local file is newer, updating S3: " << file->path << "\n";
                engine_->uploadFile(file->path);
            } else {
                std::cout << "[SyncWorker] Updating local file: " << file->path << "\n";
                std::string buffer;
                engine_->s3Provider_->downloadToBuffer(s3Obj->path, buffer);
                controller_->storage_->getThumbnailWorker()->enqueue(engine_, buffer, file->path, file->mime_type);
            }

            s3Map.erase(s3Obj->path.u8string());
        } else {
            std::cout << "[SyncWorker] Uploading new file: " << file->path << "\n";
            engine_->uploadFile(file->path);
        }
    }
}

void SyncTask::cacheDiff(std::unordered_map<std::u8string, std::shared_ptr<types::File> >& s3Map) const {
    for (const auto& dir : extractDirectories(uMap2Vector(s3Map))) {
        if (!database::FileQueries::directoryExists(engine_->vault_->id, dir->path)) {
            std::cout << "[SyncWorker] Creating directory: " << dir->path << "\n";
            database::FileQueries::addDirectory(dir);
        }
    }

    std::string buffer;
    for (const auto& file : uMap2Vector(s3Map)) {
        std::cout << "[SyncWorker] Caching new S3 file: " << file->path << "\n";
        buffer.clear();

        if (!engine_->s3Provider_->downloadToBuffer(file->path, buffer)) {
            std::cerr << "[SyncWorker] Failed to download file: " << file->path << "\n";
            continue;
        }

        file->vault_id = engine_->vault_->id;
        file->created_by = file->last_modified_by = engine_->vault_->owner_id;
        file->mime_type = getMimeType(file->path);
        if (!file->path.has_parent_path()) file->parent_id = std::nullopt;
        else file->parent_id = database::FileQueries::getDirectoryIdByPath(engine_->vault_->id, file->path.parent_path());

        controller_->storage_->getThumbnailWorker()->enqueue(engine_, buffer, file->path, file->mime_type);

        database::FileQueries::addFile(file);
    }
}

std::vector<std::shared_ptr<vh::types::Directory>> SyncTask::extractDirectories(
    const std::vector<std::shared_ptr<types::File>>& files) const {

    std::unordered_map<std::u8string, std::shared_ptr<types::Directory>> directories;

    for (const auto& file : files) {
        std::filesystem::path current;
        std::filesystem::path full_path = file->path.parent_path();

        for (const auto& part : full_path) {
            current /= part;

            auto dir_str = current.u8string();
            if (!directories.contains(dir_str)) {
                auto dir = std::make_shared<types::Directory>();
                dir->path = current;
                dir->name = current.filename().string();
                dir->created_by = dir->last_modified_by = engine_->vault_->owner_id;
                dir->vault_id = engine_->vault_->id;

                // Optionally: get parent_id if current has a parent
                const auto parent_path = current.has_parent_path() ? current.parent_path() : std::filesystem::path("/");
                dir->parent_id = database::FileQueries::getDirectoryIdByPath(engine_->vault_->id, parent_path);

                directories[dir_str] = dir;
            }
        }
    }

    // Extract and sort by depth (shallowest first)
    std::vector<std::shared_ptr<types::Directory>> result;
    for (const auto& [_, dir] : directories)
        result.push_back(dir);

    std::ranges::sort(result, [](const auto& a, const auto& b) {
        return std::distance(a->path.begin(), a->path.end()) < std::distance(b->path.begin(), b->path.end());
    });

    return result;
}

std::vector<std::shared_ptr<vh::types::File> > SyncTask::uMap2Vector(std::unordered_map<std::u8string, std::shared_ptr<types::File> >& map) {
    std::vector<std::shared_ptr<types::File>> files;
    files.reserve(map.size());
    std::ranges::transform(map.begin(), map.end(), std::back_inserter(files),
                   [](const auto& pair) { return pair.second; });
    return files;
}

std::string SyncTask::getMimeType(const std::filesystem::path& path) {
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

bool SyncTask::operator<(const SyncTask& other) const {
    return next_run > other.next_run;
}

void SyncTask::downloadDiff(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const {
    for (const auto& dir : extractDirectories(uMap2Vector(s3Map))) {
        if (!database::FileQueries::directoryExists(engine_->vault_->id, dir->path)) {
            std::cout << "[SyncWorker] Creating directory: " << dir->path << "\n";
            // TODO: determine if we need to create the directory in the local filesystem
            database::FileQueries::addDirectory(dir);
        }
    }

    for (const auto& file : uMap2Vector(s3Map)) {
        std::cout << "[SyncWorker] Downloading new S3 file: " << file->path << "\n";
        const auto absPath = engine_->getAbsolutePath(file->path);

        // TODO: add check for vault config using buffer
        if (!std::filesystem::exists(absPath.parent_path())) {
            std::cout << "[SyncWorker] Creating parent directory: " << absPath.parent_path() << "\n";
            std::filesystem::create_directories(absPath.parent_path());
        }

        // TODO: if user config is set to mirror full size, we should download the full file
        if (!engine_->s3Provider_->downloadObject(file->path, absPath)) {
            std::cerr << "[SyncWorker] Failed to download file: " << file->path << "\n";
            continue;
        }

        if (!std::filesystem::exists(absPath)) throw std::runtime_error("File does not exist at path: " + absPath.string());
        if (!std::filesystem::is_regular_file(absPath)) throw std::runtime_error("Path is not a regular file: " + absPath.string());

        const auto f = std::make_shared<types::File>();
        f->vault_id = engine_->vault_->id;
        f->name = file->path.filename();
        f->size_bytes = std::filesystem::file_size(absPath);
        f->created_by = f->last_modified_by = engine_->vault_->owner_id;
        f->path = file->path;
        f->mime_type = util::Magic::get_mime_type(absPath);
        f->content_hash = crypto::Hash::blake2b(absPath);
        if (!file->path.has_parent_path()) f->parent_id = std::nullopt;
        else f->parent_id = database::FileQueries::getDirectoryIdByPath(engine_->vault_->id, file->path.parent_path());

        database::FileQueries::addFile(f);
    }
}

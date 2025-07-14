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

    std::cout << "[SyncWorker] Starting sync for vault: " << engine_->vault_->name << std::endl;
    database::SyncQueries::reportSyncStarted(engine_->proxySync_->id);

    auto s3Map = types::groupEntriesByPath(types::filesFromS3XML(engine_->s3Provider_->listObjects()));
    sync(s3Map);
    downloadDiff(s3Map);

    std::cout << "[SyncWorker] Sync completed successfully for vault: " << engine_->vault_->name << std::endl;

    database::SyncQueries::reportSyncSuccess(engine_->proxySync_->id);
    controller_->pq.push(std::const_pointer_cast<SyncTask>(shared_from_this()));
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
                engine_->s3Provider_->downloadObject(s3Obj->path, file->path);
            }

            s3Map.erase(s3Obj->path.u8string());
        } else {
            std::cout << "[SyncWorker] Uploading new file: " << file->path << "\n";
            engine_->uploadFile(file->path);
        }
    }
}


void SyncTask::downloadDiff(std::unordered_map<std::u8string, std::shared_ptr<types::File>>& s3Map) const {
    for (const auto& file : uMap2Vector(s3Map)) {
        std::cout << "[SyncWorker] Downloading new S3 file: " << file->path << "\n";
        const auto absPath = engine_->getAbsolutePath(file->path);

        if (!std::filesystem::exists(absPath.parent_path())) {
            std::cout << "[SyncWorker] Creating parent directory: " << absPath.parent_path() << "\n";
            std::filesystem::create_directories(absPath.parent_path());
        }

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
        if (!file->path.has_parent_path() || file->path.parent_path().string() == "/") f->parent_id = std::nullopt;
        else f->parent_id = database::FileQueries::getDirectoryIdByPath(engine_->vault_->id, file->path.parent_path());

        database::FileQueries::updateFile(f);
    }
}

std::vector<std::shared_ptr<vh::types::File> > SyncTask::uMap2Vector(std::unordered_map<std::u8string, std::shared_ptr<types::File> >& map) {
    std::vector<std::shared_ptr<types::File>> files;
    files.reserve(map.size());
    std::ranges::transform(map.begin(), map.end(), std::back_inserter(files),
                   [](const auto& pair) { return pair.second; });
    return files;
}


bool SyncTask::operator<(const SyncTask& other) const {
    return next_run > other.next_run;
}


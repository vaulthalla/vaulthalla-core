#pragma once

#include "concurrency/sync/CacheSyncTask.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/CacheQueries.hpp"
#include "storage/StorageManager.hpp"
#include "services/ThumbnailWorker.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"

#include <optional>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

void CacheSyncTask::sync(std::unordered_map<std::u8string, std::shared_ptr<File> >& s3Map) const {
    for (const auto& file : FileQueries::listFilesInDir(engine_->vaultId(), {}, true)) {
        const auto match = s3Map.find(file->path.u8string());
        if (match != s3Map.end()) {
            const auto rFile = match->second;

            if (file->content_hash == rFile->content_hash) {
                s3Map.erase(match);
                continue;
            }

            if (free_ < rFile->size_bytes) {
                const auto numFileIndices = CacheQueries::countCacheIndices(vaultId(), std::make_optional(CacheIndex::Type::File));
                intmax_t needed = rFile->size_bytes - free_;
                std::vector<std::shared_ptr<CacheIndex>> purgeable;
                unsigned int numRequested = 1;

                do {
                    purgeable = CacheQueries::nLargestCacheIndicesByType(numRequested, vaultId(), CacheIndex::Type::File);
                    if (sumIndicesSize(purgeable) >= needed) break;
                    numRequested *= 2;
                } while (numRequested < numFileIndices);

                if (numRequested >= numFileIndices) {
                    std::cerr << "[CacheSyncTask] Not enough purgeable cache indices to free up space for file: " << rFile->path << "\n";
                    throw std::runtime_error("Not enough space to cache file: " + rFile->path.string());
                }

                for (const auto& index : purgeable) {
                    if (!std::filesystem::remove(index->path)) {
                        std::cerr << "[CacheSyncTask] Failed to remove cache index file: " << index->path << "\n";
                        continue;
                    }
                    CacheQueries::deleteCacheIndex(index->id);
                    free_ += index->size;
                    needed -= index->size;
                    if (needed <= 0) break;
                }
            }

            if (file->updated_at <= rFile->updated_at) {
                if (shouldPurgeNewFiles()) engine_->indexAndDeleteFile(file->path);
                else free_ -= engine_->cacheFile(file->path)->size;
                s3Map.erase(match);
                continue;
            }

            std::cout << "[CacheSyncTask][1/2] Local file is newer than remote copy: " << file->path << "\n"
                      << "[CacheSyncTask][2/2] Assuming an upload is scheduled, skipping download.\n";

            s3Map.erase(match);
        }
    }
}

void CacheSyncTask::handleDiff(std::unordered_map<std::u8string, std::shared_ptr<File> >& s3Map) const {
    for (const auto& dir : engine_->extractDirectories(uMap2Vector(s3Map))) {
        if (!DirectoryQueries::directoryExists(engine_->vaultId(), dir->path)) {
            std::cout << "[CacheSyncTask] Creating directory: " << dir->path << "\n";
            DirectoryQueries::addDirectory(dir);
        }
    }

    std::string buffer;
    for (const auto& file : uMap2Vector(s3Map)) {
        if (shouldPurgeNewFiles()) engine_->indexAndDeleteFile(file->path);
        else free_ -= engine_->cacheFile(file->path)->size;
    }
}

uintmax_t CacheSyncTask::sumIndicesSize(const std::vector<std::shared_ptr<CacheIndex> >& indices) {
    uintmax_t sum = 0;
    for (const auto& index : indices) sum += index->size;
    return sum;
}

bool CacheSyncTask::shouldPurgeNewFiles() const {
    return free_ < StorageEngine::MIN_FREE_SPACE * 2;  // 20MB threshold
}


#pragma once

#include "concurrency/sync/CacheSyncTask.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "concurrency/ThreadPoolRegistry.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/CacheQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "../../../include/concurrency/thumbnail/ThumbnailWorker.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"

#include <optional>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

void CacheSyncTask::sync(std::unordered_map<std::u8string, std::shared_ptr<File> >& s3Map) const {
    for (const auto& file : FileQueries::listFilesInDir(engine_->vaultId())) {
        const auto match = s3Map.find(stripLeadingSlash(file->path));

        if (match == s3Map.end()) {
            std::cout << "[CacheSyncTask] Local file not found in S3 map, caching: " << file->path << "\n";
            engine_->uploadFile(file->path);
            continue;
        }

        const auto rFile = match->second;

        if (file->content_hash == engine_->getRemoteContentHash(rFile->path)) {
            s3Map.erase(match);
            continue;
        }

        {
            std::unique_lock lock(engine_->mutex_);
            ensureFreeSpace(rFile->size_bytes);

            if (file->updated_at <= rFile->updated_at) {
                engine_->cacheFile(file->path)->size;
                s3Map.erase(match);
                continue;
            }
        }

        std::cout << "[CacheSyncTask][1/2] Local file is newer than remote copy: " << file->path << "\n"
            << "[CacheSyncTask][2/2] Assuming an upload is scheduled, skipping download.\n";

        s3Map.erase(match);
    }
}

void CacheSyncTask::handleDiff(std::unordered_map<std::u8string, std::shared_ptr<File> >& s3Map) const {
    for (const auto& dir : engine_->extractDirectories(uMap2Vector(s3Map))) {
        if (!DirectoryQueries::directoryExists(engine_->vaultId(), dir->path)) {
            std::cout << "[CacheSyncTask] Creating directory: " << dir->path << "\n";
            dir->parent_id = DirectoryQueries::getDirectoryIdByPath(engine_->vaultId(), dir->path.parent_path());
            DirectoryQueries::addDirectory(dir);
        }
    }

    const auto files = uMap2Vector(s3Map);
    std::vector<std::future<ExpectedFuture>> futures;

    const auto freeSpace = engine_->freeSpace();
    const auto reqFreeSpace = computeReqFreeSpaceForDownload(files);
    const auto [purgeableSpace, maxSize] = computeIndicesSizeAndMaxSize(CacheQueries::listCacheIndicesByType(vaultId(), CacheIndex::Type::File));
    const bool freeAfterDownload = freeSpace + purgeableSpace < reqFreeSpace;

    if (freeAfterDownload) {
        std::cout << "[CacheSyncTask] Not enough free space for download, purging cache indices.\n";
        ensureFreeSpace(maxSize);
    }

    const auto threadPool = ThreadPoolRegistry::instance().syncPool();
    for (const auto& file : files) {
        auto task = std::make_shared<DownloadTask>(engine_, file, freeAfterDownload);
        futures.push_back(task->getFuture().value());
        threadPool->submit(task);
    }

    for (auto& f : futures)
        if (std::get<bool>(f.get()) == false) std::cerr << "[CacheSyncTask] A file sync task failed.\n";
}

std::pair<uintmax_t, uintmax_t> CacheSyncTask::computeIndicesSizeAndMaxSize(const std::vector<std::shared_ptr<CacheIndex> >& indices) {
    uintmax_t sum = 0;
    uintmax_t maxSize = 0;
    for (const auto& index : indices) {
        sum += index->size;
        if (index->size > maxSize) maxSize = index->size;
    }
    return {sum, maxSize};
}

void CacheSyncTask::ensureFreeSpace(const uintmax_t size) const {
    auto free = engine_->freeSpace();
    if (engine_->getVault()->quota != 0 && free < size) {
        const auto numFileIndices = CacheQueries::countCacheIndices(
            vaultId(), std::make_optional(CacheIndex::Type::File));

        if (numFileIndices == 0) throw std::runtime_error("Not enough space to cache file");

        intmax_t needed = size - free;
        std::vector<std::shared_ptr<CacheIndex> > purgeable;
        unsigned int numRequested = 1;

        do {
            purgeable = CacheQueries::nLargestCacheIndicesByType(numRequested, vaultId(), CacheIndex::Type::File);
            if (computeIndicesSizeAndMaxSize(purgeable).first >= needed) break;
            numRequested *= 2;
        } while (numRequested < numFileIndices);

        if (numRequested >= numFileIndices) throw std::runtime_error("Not enough space to cache file");

        for (const auto& index : purgeable) {
            if (!std::filesystem::remove(index->path)) {
                std::cerr << "[CacheSyncTask] Failed to remove cache index file: " << index->path << "\n";
                continue;
            }

            CacheQueries::deleteCacheIndex(index->id);
            free += index->size;
            needed -= index->size;
            if (needed <= 0) break;
        }
    }
}

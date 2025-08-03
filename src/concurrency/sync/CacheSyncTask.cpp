#include "concurrency/sync/CacheSyncTask.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/ThreadPool.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/CacheQueries.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "types/Vault.hpp"
#include "types/Path.hpp"
#include "util/fsPath.hpp"

#include <optional>
#include <iostream>

using namespace vh::concurrency;
using namespace vh::storage;
using namespace vh::types;
using namespace vh::database;

void CacheSyncTask::sync() {
    for (const auto& file : localFiles_) {
        const auto strippedPath = stripLeadingSlash(file->path).u8string();
        const auto match = s3Map_.find(strippedPath);

        if (match == s3Map_.end()) {
            std::cout << "[CacheSyncTask] Local file not found in S3 map, caching: " << file->path << "\n";
            upload(file);
            continue;
        }

        const auto rFile = match->second;
        const auto remoteHash = std::make_optional(cloudEngine()->getRemoteContentHash(rFile->path));

        if (file->content_hash && remoteHashMap_[strippedPath] && *file->content_hash == remoteHashMap_[strippedPath]) {
            s3Map_.erase(match);
            continue;
        }

        if (file->updated_at <= rFile->updated_at) {
            ensureFreeSpace(rFile->size_bytes);
            download(file);
            s3Map_.erase(match);
            continue;
        }

        std::cout << "[CacheSyncTask][1/2] Local file is newer than remote copy: " << file->path << "\n"
            << "[CacheSyncTask][2/2] Assuming an upload is scheduled, skipping download.\n";

        s3Map_.erase(match);
    }

    processFutures();

    // Ensure all directories in the S3 map exist locally
    for (const auto& dir : cloudEngine()->extractDirectories(uMap2Vector(s3Map_))) {
        if (!DirectoryQueries::directoryExists(engine_->vault->id, dir->path)) {
            std::cout << "[CacheSyncTask] Creating directory: " << dir->path << "\n";
            dir->parent_id = DirectoryQueries::getDirectoryIdByPath(engine_->vault->id, dir->path.parent_path());
            if (dir->fuse_path.empty()) dir->fuse_path = engine_->paths->absPath(dir->path, PathType::VAULT_ROOT);
            DirectoryQueries::upsertDirectory(dir);
        }
    }

    const auto files = uMap2Vector(s3Map_);
    futures_.reserve(files.size());

    const auto freeSpace = engine_->freeSpace();
    const auto reqFreeSpace = computeReqFreeSpaceForDownload(files);
    const auto [purgeableSpace, maxSize] =
        computeIndicesSizeAndMaxSize(CacheQueries::listCacheIndicesByType(vaultId(), CacheIndex::Type::File));
    const bool freeAfterDownload = freeSpace + purgeableSpace < reqFreeSpace;

    if (freeAfterDownload) ensureFreeSpace(maxSize);

    for (const auto& file : files) download(file, freeAfterDownload);

    processFutures();
}

std::pair<uintmax_t, uintmax_t>
CacheSyncTask::computeIndicesSizeAndMaxSize(const std::vector<std::shared_ptr<CacheIndex> >& indices) {
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
    if (engine_->vault->quota != 0 && free < size) {
        const auto numFileIndices = CacheQueries::countCacheIndices(
            vaultId(), std::make_optional(CacheIndex::Type::File));

        if (numFileIndices == 0) throw std::runtime_error("Not enough space to cache file");

        uintmax_t needed = size - free;
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

#include "concurrency/sync/SyncTask.hpp"

#include "storage/cloud/CloudStorageEngine.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/sync/UploadTask.hpp"
#include "concurrency/sync/CloudDeleteTask.hpp"
#include "concurrency/sync/CloudTrashedDeleteTask.hpp"
#include "concurrency/sync/CloudRotateKeyTask.hpp"
#include "concurrency/sync/SyncExecutor.hpp"

#include "types/vault/Vault.hpp"
#include "database/Queries/FileQueries.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "types/fs/File.hpp"
#include "types/fs/Directory.hpp"
#include "types/fs/Path.hpp"

#include "logging/LogRegistry.hpp"

#include "types/sync/Event.hpp"
#include "types/sync/Throughput.hpp"
#include "types/sync/Artifact.hpp"
#include "types/sync/ConflictArtifact.hpp"
#include "types/sync/Conflict.hpp"
#include "types/sync/RemotePolicy.hpp"
#include "types/sync/helpers.hpp"
#include "types/sync/Planner.hpp"

#include "util/fsPath.hpp"
#include "crypto/IdGenerator.hpp"

#include <utility>

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::database;
using namespace std::chrono;
using namespace vh::logging;
using namespace vh::storage;


// ##########################################
// ########### FSTask Overrides #############
// ##########################################

void SyncTask::operator()() {
    startTask();

    const Stage stages[] = {
        {"shared",   [this]{ processSharedOps(); }},
        {"initBins", [this]{ initBins(); }},
        {"sync",     [this]{ sync(); }},
        {"clearBins",[this]{ clearBins(); }},
    };

    runStages(stages);
    shutdown();
}

void SyncTask::removeTrashedFiles() {
    const auto files = FileQueries::listTrashedFiles(vaultId());

    futures.reserve(files.size());
    for (const auto& file : files)
        push(std::make_shared<CloudTrashedDeleteTask>(
            cloudEngine(), file, op(sync::Throughput::Metric::DELETE)));

    processFutures();
}

void SyncTask::pushKeyRotationTask(const std::vector<std::shared_ptr<File> >& files,
                                  unsigned int begin,
                                  unsigned int end) {
    push(std::make_shared<CloudRotateKeyTask>(cloudEngine(), files, begin, end));
}

// ##########################################
// ############# Sync Operations ############
// ##########################################

void SyncTask::sync() {
    const auto self = std::static_pointer_cast<SyncTask>(shared_from_this());
    SyncExecutor::run(self, sync::Planner::build(self, cloudEngine()->remote_policy()));
}

void SyncTask::initBins() {
    s3Map = cloudEngine()->getGroupedFilesFromS3();
    s3Files = uMap2Vector(s3Map);

    localFiles = FileQueries::listFilesInDir(engine->vault->id);
    localMap = groupEntriesByPath(localFiles);

    event->heartbeat();

    for (const auto& [path, entry] : intersect(s3Map, localMap))
        remoteHashMap.insert({entry->path.u8string(),
                              cloudEngine()->getRemoteContentHash(entry->path)});
}

void SyncTask::clearBins() {
    localFiles.clear();
    s3Files.clear();
    s3Map.clear();
    localMap.clear();
    remoteHashMap.clear();
}

// ##########################################
// ########### File Operations #############
// ##########################################

void SyncTask::upload(const std::shared_ptr<File>& file) {
    push(std::make_shared<UploadTask>(
        cloudEngine(), file, op(sync::Throughput::Metric::UPLOAD)));
}

void SyncTask::download(const std::shared_ptr<File>& file, const bool freeAfterDownload) {
    push(std::make_shared<DownloadTask>(
        cloudEngine(),
        file,
        event->getOrCreateThroughput(sync::Throughput::Metric::DOWNLOAD).newOp(),
        freeAfterDownload));
}

void SyncTask::remove(const std::shared_ptr<File>& file, const CloudDeleteTask::Type& type) {
    push(std::make_shared<CloudDeleteTask>(
        cloudEngine(), file, op(sync::Throughput::Metric::DELETE), type));
}

// ##########################################
// ############ Internal Helpers ############
// ##########################################

std::shared_ptr<CloudStorageEngine> SyncTask::cloudEngine() const {
    return std::static_pointer_cast<CloudStorageEngine>(engine);
}

std::vector<sync::EntryKey> SyncTask::allKeysSorted() const {
    std::vector<sync::EntryKey> keys;
    keys.reserve(localMap.size() + s3Map.size());

    for (const auto& [k, _] : localMap) keys.push_back({k});
    for (const auto& [k, _] : s3Map)    keys.push_back({k});

    std::ranges::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

void SyncTask::ensureDirectoriesFromRemote() {
    for (const auto& dir : cloudEngine()->extractDirectories(uMap2Vector(s3Map))) {
        if (!DirectoryQueries::directoryExists(engine->vault->id, dir->path)) {
            dir->parent_id = DirectoryQueries::getDirectoryIdByPath(
                engine->vault->id, dir->path.parent_path());

            if (dir->fuse_path.empty())
                dir->fuse_path = engine->paths->absPath(dir->path, PathType::VAULT_ROOT);

            dir->base32_alias = ids::IdGenerator({ .namespace_token = dir->name }).generate();
            DirectoryQueries::upsertDirectory(dir);
        }
    }
}

// ##########################################
// ########### Conflict Handling ############
// ##########################################

bool SyncTask::hasPotentialConflict(const std::shared_ptr<File>& local,
                                   const std::shared_ptr<File>& upstream,
                                   bool upstream_decryption_failure) {
    if (upstream_decryption_failure) return true;
    if (local->size_bytes != upstream->size_bytes) return true;

    if (local->content_hash && upstream->content_hash &&
        *local->content_hash != *upstream->content_hash)
        return true;

    return false;
}

std::shared_ptr<sync::Conflict> SyncTask::maybeBuildConflict(
    const std::shared_ptr<File>& local,
    const std::shared_ptr<File>& upstream) const
{
    if (!hasPotentialConflict(local, upstream, false)) return nullptr;

    auto c = std::make_shared<sync::Conflict>();
    c->failed_to_decrypt_upstream = false; // Planner-time default
    c->artifacts.local = sync::Artifact(local, sync::Artifact::Side::LOCAL);
    c->artifacts.upstream = sync::Artifact(upstream, sync::Artifact::Side::UPSTREAM);
    c->analyze();

    if (c->reasons.empty()) {
        LogRegistry::sync()->debug(
            "[SyncTask] hasPotentialConflict() returned true, but no reasons were identified. Possible detection bug.");
        return nullptr;
    }

    c->created_at = system_clock::to_time_t(system_clock::now());
    c->file_id = local->id;
    c->event_id = event->id;

    return c;
}

bool SyncTask::handleConflict(const std::shared_ptr<sync::Conflict>& c) const {
    const bool ok = engine->sync->resolve_conflict(c);
    if (ok) c->resolved_at = system_clock::to_time_t(system_clock::now());
    event->conflicts.push_back(c);
    return !ok; // true => unresolved (Ask)
}

// ##########################################
// ########### Static Helpers ###############
// ##########################################

uintmax_t SyncTask::computeReqFreeSpaceForDownload(
    const std::vector<std::shared_ptr<File> >& files) {
    uintmax_t totalSize = 0;
    for (const auto& file : files) totalSize += file->size_bytes;
    return totalSize;
}

std::vector<std::shared_ptr<File> > SyncTask::uMap2Vector(
    std::unordered_map<std::u8string, std::shared_ptr<File> >& map)
{
    std::vector<std::shared_ptr<File> > files;
    files.reserve(map.size());

    std::ranges::transform(
        map.begin(), map.end(),
        std::back_inserter(files),
        [](const auto& pair) { return pair.second; });

    return files;
}

std::unordered_map<std::u8string, std::shared_ptr<File>> SyncTask::intersect(
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& a,
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& b)
{
    std::unordered_map<std::u8string, std::shared_ptr<File>> result;
    for (const auto& item : a)
        if (b.contains(item.first))
            result.insert(item);

    return result;
}

#include "concurrency/sync/SyncTask.hpp"
#include "storage/cloud/CloudStorageEngine.hpp"
#include "concurrency/sync/DownloadTask.hpp"
#include "concurrency/sync/UploadTask.hpp"
#include "concurrency/sync/CloudDeleteTask.hpp"
#include "concurrency/sync/CloudTrashedDeleteTask.hpp"
#include "concurrency/sync/CloudRotateKeyTask.hpp"
#include "types/vault/Vault.hpp"
#include "database/Queries/FileQueries.hpp"
#include "types/fs/File.hpp"
#include "logging/LogRegistry.hpp"
#include "types/sync/Event.hpp"
#include "types/sync/Throughput.hpp"
#include "types/sync/Artifact.hpp"
#include "types/sync/ConflictArtifact.hpp"
#include "types/sync/Conflict.hpp"
#include "types/sync/LocalPolicy.hpp"
#include "types/sync/RemotePolicy.hpp"
#include "types/sync/helpers.hpp"

#include <utility>

using namespace vh::concurrency;
using namespace vh::types;
using namespace vh::database;
using namespace std::chrono;
using namespace vh::logging;
using namespace vh::storage;

static std::u8string normalizeRel(const std::filesystem::path& p) {
    return stripLeadingSlash(p).u8string();
}

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

void SyncTask::initBins() {
    s3Map = cloudEngine()->getGroupedFilesFromS3();
    s3Files = uMap2Vector(s3Map);
    localFiles = FileQueries::listFilesInDir(engine->vault->id);
    localMap = groupEntriesByPath(localFiles);

    event->heartbeat();

    for (const auto& [path, entry] : intersect(s3Map, localMap))
        remoteHashMap.insert({entry->path.u8string(), cloudEngine()->getRemoteContentHash(entry->path)});
}

void SyncTask::pushKeyRotationTask(const std::vector<std::shared_ptr<File> >& files, unsigned int begin, unsigned int end) {
    push(std::make_shared<CloudRotateKeyTask>(cloudEngine(), files, begin, end));
}

void SyncTask::removeTrashedFiles() {
    const auto files = FileQueries::listTrashedFiles(vaultId());

    futures.reserve(files.size());
    for (const auto& file : files)
        push(std::make_shared<CloudTrashedDeleteTask>(cloudEngine(), file, op(sync::Throughput::Metric::DELETE)));

    processFutures();
}

void SyncTask::upload(const std::shared_ptr<File>& file) {
    push(std::make_shared<UploadTask>(cloudEngine(), file, op(sync::Throughput::Metric::UPLOAD)));
}

void SyncTask::download(const std::shared_ptr<File>& file, const bool freeAfterDownload) {
    push(std::make_shared<DownloadTask>(cloudEngine(), file, event->getOrCreateThroughput(sync::Throughput::Metric::DOWNLOAD).newOp(), freeAfterDownload));
}

void SyncTask::remove(const std::shared_ptr<File>& file, const CloudDeleteTask::Type& type) {
    push(std::make_shared<CloudDeleteTask>(cloudEngine(), file, op(sync::Throughput::Metric::DELETE), type));
}

bool SyncTask::hasPotentialConflict(const std::shared_ptr<File>& local, const std::shared_ptr<File>& upstream, bool upstream_decryption_failure) {
    if (upstream_decryption_failure) return true;

    if (local->size_bytes != upstream->size_bytes) return true;

    if (local->content_hash && upstream->content_hash && *local->content_hash != *upstream->content_hash) return true;

    return false;
}


bool SyncTask::conflict(const std::shared_ptr<File>& local, const std::shared_ptr<File>& upstream, const bool upstream_decryption_failure) const {
    if (!hasPotentialConflict(local, upstream, upstream_decryption_failure)) return false;

    const auto conflict = std::make_shared<sync::Conflict>();
    conflict->failed_to_decrypt_upstream = upstream_decryption_failure;
    conflict->artifacts.local = sync::Artifact(local, sync::Artifact::Side::LOCAL);
    conflict->artifacts.upstream = sync::Artifact(upstream, sync::Artifact::Side::UPSTREAM);
    conflict->analyze();

    if (conflict->reasons.empty()) {
        LogRegistry::sync()->debug("[SyncTask] hasPotentialConflict() returned true, but no reasons were identified for the conflict. This might indicate a logic error in the conflict detection algorithm.");
        return false;
    }

    conflict->created_at = system_clock::to_time_t(system_clock::now());
    conflict->file_id = local->id;
    conflict->event_id = event->id;

    const bool ok = engine->sync->resolve_conflict(conflict);

    if (ok) conflict->resolved_at = system_clock::to_time_t(system_clock::now());
    event->conflicts.push_back(conflict);

    return !ok;
}

uintmax_t SyncTask::computeReqFreeSpaceForDownload(const std::vector<std::shared_ptr<File> >& files) {
    uintmax_t totalSize = 0;
    for (const auto& file : files) totalSize += file->size_bytes;
    return totalSize;
}

std::shared_ptr<CloudStorageEngine> SyncTask::cloudEngine() const { return std::static_pointer_cast<CloudStorageEngine>(engine); }

std::vector<std::shared_ptr<File> > SyncTask::uMap2Vector(
    std::unordered_map<std::u8string, std::shared_ptr<File> >& map) {

    std::vector<std::shared_ptr<File> > files;
    files.reserve(map.size());
    std::ranges::transform(map.begin(), map.end(), std::back_inserter(files),
                           [](const auto& pair) { return pair.second; });
    return files;
}

void SyncTask::ensureFreeSpace(const uintmax_t size) const {
    if (engine->vault->quota != 0 && engine->freeSpace() < size)
        throw std::runtime_error("Not enough space to cache file");
}

sync::CompareResult SyncTask::compareLocalRemote(const std::shared_ptr<File>& L, const std::shared_ptr<File>& R) const {
    // Ensure remote hash exists (lazy)
    if (!R->content_hash) R->content_hash = cloudEngine()->getRemoteContentHash(R->path);

    const auto rel = normalizeRel(L->path);
    const auto remoteKnown = remoteHashMap.contains(rel) ? remoteHashMap.at(rel) : std::optional<std::string>{};

    bool equal = false;
    if (L->content_hash && !remoteKnown) equal = (*L->content_hash == remoteKnown);
    else if (L->content_hash && R->content_hash) equal = (*L->content_hash == *R->content_hash);

    const auto localNewer  = L->updated_at > R->updated_at;
    const auto remoteNewer = L->updated_at < R->updated_at;

    return { equal, localNewer, remoteNewer };
}

std::vector<sync::EntryKey> SyncTask::allKeysSorted() const {
    std::vector<sync::EntryKey> keys;
    keys.reserve(localMap.size() + s3Map.size());
    for (auto& [k,_] : localMap) keys.push_back({k});
    for (auto& [k,_] : s3Map) keys.push_back({k});
    std::ranges::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());
    return keys;
}

void SyncTask::ensureDirectoriesFromRemote() {
    for (const auto& dir : cloudEngine()->extractDirectories(uMap2Vector(s3Map))) {
        if (!DirectoryQueries::directoryExists(engine->vault->id, dir->path)) {
            dir->parent_id = DirectoryQueries::getDirectoryIdByPath(engine->vault->id, dir->path.parent_path());
            if (dir->fuse_path.empty()) dir->fuse_path = engine->paths->absPath(dir->path, PathType::VAULT_ROOT);
            dir->base32_alias = ids::IdGenerator({ .namespace_token = dir->name }).generate();
            DirectoryQueries::upsertDirectory(dir);
        }
    }
}

std::shared_ptr<sync::Conflict> SyncTask::maybeBuildConflict(const std::shared_ptr<File>& local,
                             const std::shared_ptr<File>& upstream) const
{
    if (!hasPotentialConflict(local, upstream, false)) return nullptr;

    auto c = std::make_shared<sync::Conflict>();
    c->failed_to_decrypt_upstream = false; // This is called during Planner.build(), this may update later
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
    const bool ok = engine->sync->resolve_conflict(c); // your policy virtual

    if (ok) c->resolved_at = system_clock::to_time_t(system_clock::now());

    event->conflicts.push_back(c);

    return !ok; // true means unresolved -> Ask
}

std::unordered_map<std::u8string, std::shared_ptr<File>> SyncTask::symmetric_diff(
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& a,
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& b) {
    std::unordered_map<std::u8string, std::shared_ptr<File>> result;

    for (const auto& item : a) if (!b.contains(item.first)) result.insert(item);
    for (const auto& item : b) if (!a.contains(item.first)) result.insert(item);

    return result;
}

std::unordered_map<std::u8string, std::shared_ptr<File>> SyncTask::intersect(
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& a,
    const std::unordered_map<std::u8string, std::shared_ptr<File> >& b) {
    std::unordered_map<std::u8string, std::shared_ptr<File>> result;

    for (const auto& item : a) if (b.contains(item.first)) result.insert(item);

    return result;
}

void SyncTask::clearBins() {
    localFiles.clear();
    s3Files.clear();
    s3Map.clear();
    localMap.clear();
    remoteHashMap.clear();
}

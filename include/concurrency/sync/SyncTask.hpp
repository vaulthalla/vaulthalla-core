#pragma once

#include "concurrency/FSTask.hpp"
#include "CloudDeleteTask.hpp"
#include "types/sync/helpers.hpp"

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

namespace vh::types {
struct File;

namespace sync {
struct ScopedOp;
struct Conflict;
}}

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::concurrency {

struct SyncTask : public FSTask {
    ~SyncTask() override = default;

    explicit SyncTask(const std::shared_ptr<storage::StorageEngine>& engine) : FSTask(engine) {
    }

    void operator()() override;

    virtual void sync() = 0;

    std::vector<std::shared_ptr<types::File>> localFiles, s3Files;
    std::unordered_map<std::u8string, std::shared_ptr<types::File>> localMap, s3Map;
    std::unordered_map<std::u8string, std::optional<std::string>> remoteHashMap;

    void upload(const std::shared_ptr<types::File>& file);

    void download(const std::shared_ptr<types::File>& file, bool freeAfterDownload = false);

    void remove(const std::shared_ptr<types::File>& file,
                const CloudDeleteTask::Type& type = CloudDeleteTask::Type::PURGE);

    std::shared_ptr<storage::CloudStorageEngine> cloudEngine() const;

    void removeTrashedFiles() override;

    void pushKeyRotationTask(const std::vector<std::shared_ptr<types::File> >& files,
                             unsigned int begin, unsigned int end) override;

    virtual void ensureFreeSpace(uintmax_t size) const;

    [[nodiscard]] static bool hasPotentialConflict(const std::shared_ptr<types::File>& local, const std::shared_ptr<types::File>& upstream, bool upstream_decryption_failure);

    [[nodiscard]] bool conflict(const std::shared_ptr<types::File>& local, const std::shared_ptr<types::File>& upstream, bool upstream_decryption_failure = false) const;

    static uintmax_t computeReqFreeSpaceForDownload(const std::vector<std::shared_ptr<types::File> >& files);

    static std::vector<std::shared_ptr<types::File> > uMap2Vector(
        std::unordered_map<std::u8string, std::shared_ptr<types::File> >& map);

    static std::unordered_map<std::u8string, std::shared_ptr<types::File> > intersect(
        const std::unordered_map<std::u8string, std::shared_ptr<types::File> >& a,
        const std::unordered_map<std::u8string, std::shared_ptr<types::File> >& b);

    static std::unordered_map<std::u8string, std::shared_ptr<types::File> > symmetric_diff(
        const std::unordered_map<std::u8string, std::shared_ptr<types::File> >& a,
        const std::unordered_map<std::u8string, std::shared_ptr<types::File> >& b);

    void initBins();

    void clearBins();

    types::sync::CompareResult compareLocalRemote(const std::shared_ptr<types::File>& L, const std::shared_ptr<types::File>& R) const;

    std::vector<types::sync::EntryKey> allKeysSorted() const;

    void ensureDirectoriesFromRemote();

    std::shared_ptr<types::sync::Conflict> maybeBuildConflict(const std::shared_ptr<types::File>& local,
                             const std::shared_ptr<types::File>& upstream) const;

    bool handleConflict(const std::shared_ptr<types::sync::Conflict>& c) const;
};

}
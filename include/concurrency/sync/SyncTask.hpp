#pragma once

#include "concurrency/FSTask.hpp"
#include "CloudDeleteTask.hpp"

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

namespace vh::types {
struct File;

namespace sync {
struct ScopedOp;
}}

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::concurrency {

class SyncTask : public FSTask {
public:
    ~SyncTask() override = default;

    explicit SyncTask(const std::shared_ptr<storage::StorageEngine>& engine) : FSTask(engine) {
    }

    void operator()() override;

    virtual void sync() = 0;

protected:
    std::vector<std::shared_ptr<types::File> > localFiles_, s3Files_;
    std::unordered_map<std::u8string, std::shared_ptr<types::File> > localMap_, s3Map_;
    std::unordered_map<std::u8string, std::optional<std::string> > remoteHashMap_;

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

private:
    void initBins();

    void clearBins();
};

}
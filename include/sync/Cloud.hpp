#pragma once

#include "Local.hpp"
#include "sync/tasks/Delete.hpp"
#include "model/helpers.hpp"

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

namespace vh::types {
struct File;
}

namespace vh::sync::model {
struct Conflict;
}

namespace vh::storage {
class CloudStorageEngine;
}

namespace vh::sync {

struct Cloud final : Local {
    std::vector<std::shared_ptr<types::File>> localFiles, s3Files;
    std::unordered_map<std::u8string, std::shared_ptr<types::File>> localMap, s3Map;
    std::unordered_map<std::u8string, std::optional<std::string>> remoteHashMap;

    ~Cloud() override = default;
    explicit Cloud(const std::shared_ptr<storage::StorageEngine>& engine) : Local(engine) {}


    // ##########################################
    // ########### FSTask Overrides #############
    // ##########################################

    void operator()() override;


    // ##########################################
    // ############# Sync Operations ############
    // ##########################################

    void sync();
    void initBins();
    void clearBins();


    // ##########################################
    // ########### File Operations #############
    // ##########################################

    void upload(const std::shared_ptr<types::File>& file);
    void download(const std::shared_ptr<types::File>& file, bool freeAfterDownload = false);
    void remove(const std::shared_ptr<types::File>& file,
                const tasks::Delete::Type& type = tasks::Delete::Type::PURGE);


    // ##########################################
    // ############ Internal Helpers ############
    // ##########################################

    std::shared_ptr<storage::CloudStorageEngine> cloudEngine() const;
    std::vector<model::EntryKey> allKeysSorted() const;
    void ensureDirectoriesFromRemote();


    // ##########################################
    // ########### Conflict Handling ############
    // ##########################################

    [[nodiscard]] static bool hasPotentialConflict(const std::shared_ptr<types::File>& local, const std::shared_ptr<types::File>& upstream, bool upstream_decryption_failure);

    std::shared_ptr<model::Conflict> maybeBuildConflict(const std::shared_ptr<types::File>& local,
                             const std::shared_ptr<types::File>& upstream) const;

    bool handleConflict(const std::shared_ptr<model::Conflict>& c) const;


    // ##########################################
    // ########### Static Helpers ###############
    // ##########################################

    static uintmax_t computeReqFreeSpaceForDownload(const std::vector<std::shared_ptr<types::File> >& files);

    static std::vector<std::shared_ptr<types::File> > uMap2Vector(
        std::unordered_map<std::u8string, std::shared_ptr<types::File> >& map);

    static std::unordered_map<std::u8string, std::shared_ptr<types::File> > intersect(
        const std::unordered_map<std::u8string, std::shared_ptr<types::File> >& a,
        const std::unordered_map<std::u8string, std::shared_ptr<types::File> >& b);
};

}
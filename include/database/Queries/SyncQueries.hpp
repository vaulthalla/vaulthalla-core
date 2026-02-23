#pragma once

#include <memory>

namespace vh::sync::model {
struct Policy;
}

namespace vh::database {

struct SyncQueries {
    static void addSync(const std::shared_ptr<sync::model::Policy>& sync);
    static void updateSync(const std::shared_ptr<sync::model::Policy>& sync);
    static void deleteSync(unsigned int syncId);
    static void deleteSyncbyVaultId(unsigned int vaultId);
    static std::shared_ptr<sync::model::Policy> getSync(unsigned int vaultId);
    static void reportSyncStarted(unsigned int syncId);
    static void reportSyncSuccess(unsigned int syncId);
};

}

#pragma once

#include <memory>

namespace vh::types {
struct ProxySync;
}

namespace vh::database {

struct SyncQueries {
    static std::shared_ptr<types::ProxySync> getProxySyncConfig(unsigned int vaultId);
    static void reportSyncStarted(unsigned int syncId);
    static void reportSyncSuccess(unsigned int syncId);
};

}

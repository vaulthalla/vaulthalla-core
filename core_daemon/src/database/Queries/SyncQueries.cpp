#include "database/Queries/SyncQueries.hpp"
#include "database/Transactions.hpp"
#include "types/ProxySync.hpp"

namespace vh::database {

std::shared_ptr<types::ProxySync> SyncQueries::getProxySyncConfig(const unsigned int vaultId) {
    return Transactions::exec("SyncQueries::getProxySyncConfig", [&](pqxx::work& txn) {
        const auto row = txn.exec_prepared("get_proxy_sync_config", vaultId).one_row();
        return std::make_shared<types::ProxySync>(row);
    });
}

void SyncQueries::reportSyncStarted(const unsigned int syncId) {
    Transactions::exec("SyncQueries::reportSyncStarted", [&](pqxx::work& txn) {
        txn.exec_prepared("report_sync_started", syncId);
    });
}

void SyncQueries::reportSyncSuccess(const unsigned int syncId) {
    Transactions::exec("SyncQueries::reportSyncSuccess", [&](pqxx::work& txn) {
        txn.exec_prepared("report_sync_success", syncId);
    });
}


}

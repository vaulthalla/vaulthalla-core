#include "database/Queries/SyncQueries.hpp"
#include "database/Transactions.hpp"
#include "types/ProxySync.hpp"

#include <pqxx/row>

namespace vh::database {

std::shared_ptr<types::ProxySync> SyncQueries::getProxySyncConfig(unsigned int vaultId) {
    return Transactions::exec("SyncQueries::getProxySyncConfig", [vaultId](pqxx::work& txn) {)
        const auto row = txn.exec_prepared("get_proxy_sync_config", vaultId).one_row();
        return std::make_shared<types::ProxySync>(row);
    });
}

void SyncQueries::reportSyncStarted(unsigned int syncId) {
    Transactions::exec("SyncQueries::reportSyncStarted", [syncId](pqxx::work& txn) {
        txn.exec_prepared("report_sync_started", syncId);
    });
}

void SyncQueries::reportSyncSuccess(unsigned int syncId) {
    Transactions::exec("SyncQueries::reportSyncSuccess", [syncId](pqxx::work& txn) {
        txn.exec_prepared("report_sync_success", syncId);
    });
}


}

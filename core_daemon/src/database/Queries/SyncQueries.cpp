#include "database/Queries/SyncQueries.hpp"
#include "database/Transactions.hpp"
#include "types/Sync.hpp"

namespace vh::database {

std::shared_ptr<types::Sync> SyncQueries::getSync(const unsigned int vaultId) {
    return Transactions::exec("SyncQueries::getProxySyncConfig", [&](pqxx::work& txn) {
        const auto row = txn.exec_prepared("get_proxy_sync_config", vaultId).one_row();
        return std::make_shared<types::Sync>(row);
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

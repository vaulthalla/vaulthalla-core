#include "database/Queries/SyncQueries.hpp"
#include "database/Transactions.hpp"
#include "sync/model/RemotePolicy.hpp"
#include "sync/model/LocalPolicy.hpp"

using namespace vh::database;
using namespace vh::sync::model;

std::shared_ptr<Policy> SyncQueries::getSync(const unsigned int vaultId) {
    return Transactions::exec("SyncQueries::getProxySyncConfig", [&](pqxx::work& txn) -> std::shared_ptr<Policy> {
        const auto type = txn.exec("SELECT type FROM vault WHERE id = " + txn.quote(vaultId)).one_field().as<std::string>();

        if (type == "local")
            return std::make_shared<LocalPolicy>(
                txn.exec(pqxx::prepped{"get_fsync_config"}, vaultId).one_row());

        if (type == "s3")
            return std::make_shared<RemotePolicy>(
                txn.exec(pqxx::prepped{"get_rsync_config"}, vaultId).one_row());

        throw std::runtime_error("Unknown sync type: " + type);
    });
}

void SyncQueries::reportSyncStarted(const unsigned int syncId) {
    Transactions::exec("SyncQueries::reportSyncStarted", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"report_sync_started"}, syncId);
    });
}

void SyncQueries::reportSyncSuccess(const unsigned int syncId) {
    Transactions::exec("SyncQueries::reportSyncSuccess", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"report_sync_success"}, syncId);
    });
}

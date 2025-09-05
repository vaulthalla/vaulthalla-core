#include "database/Queries/SyncQueries.hpp"
#include "database/Transactions.hpp"
#include "types/RSync.hpp"
#include "types/FSync.hpp"

namespace vh::database {

std::shared_ptr<types::Sync> SyncQueries::getSync(const unsigned int vaultId) {
    return Transactions::exec("SyncQueries::getProxySyncConfig", [&](pqxx::work& txn) -> std::shared_ptr<types::Sync> {
        const auto type = txn.exec("SELECT type FROM vault WHERE id = " + txn.quote(vaultId)).one_field().as<std::string>();

        if (type == "local")
            return std::make_shared<types::FSync>(
                txn.exec(pqxx::prepped{"get_fsync_config"}, vaultId).one_row());

        if (type == "s3")
            return std::make_shared<types::RSync>(
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

}
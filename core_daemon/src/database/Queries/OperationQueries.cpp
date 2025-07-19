#include "database/Queries/OperationQueries.hpp"
#include "database/Transactions.hpp"
#include "types/Operation.hpp"

using namespace vh::database;
using namespace vh::types;

void OperationQueries::addOperation(const std::shared_ptr<Operation>& op) {
    Transactions::exec("OperationQueries::addOperation", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(op->fs_entry_id);
        p.append(op->executed_by);
        p.append(to_string(op->operation));
        p.append(to_string(op->target));
        p.append(to_string(op->status));
        p.append(op->source_path);

        txn.exec_prepared("insert_operation", p);
    });
}

void OperationQueries::deleteOperation(unsigned int id) {
    Transactions::exec("OperationQueries::deleteOperation", [&](pqxx::work& txn) {
        txn.exec_prepared("delete_operation", id);
    });
}

std::vector<std::shared_ptr<Operation>> OperationQueries::listOperationsByVault(unsigned int vaultId) {
    return Transactions::exec("OperationQueries::listOperationsByVault", [&](pqxx::work& txn) {
        pqxx::params p{vaultId};
        const auto res = txn.exec_prepared("list_pending_operations_by_vault", p);
        return operations_from_pq_res(res);
    });
}

void OperationQueries::markOperationCompleted(const std::shared_ptr<Operation>& op) {
    Transactions::exec("OperationQueries::markOperationCompleted", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(op->id);
        p.append(to_string(op->status));
        p.append(op->error.value_or(""));

        txn.exec_prepared("mark_operation_completed_and_update", p);
    });
}

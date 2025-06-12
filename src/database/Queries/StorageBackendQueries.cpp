#include "database/Queries/StorageBackendQueries.hpp"
#include "database/Transactions.hpp"

namespace vh::database {
    void StorageBackendQueries::addStorageBackend(const vh::types::StorageBackend& backend) {
        Transactions::exec("StorageBackendQueries::addStorageBackend", [&](pqxx::work& txn) {
            txn.exec("INSERT INTO storage_backends (name, type, is_active, created_at) VALUES ("
                     + txn.quote(backend.name) + ", "
                     + txn.quote(static_cast<int>(backend.type)) + ", "
                     + txn.quote(backend.is_active) + ", "
                     + txn.quote(backend.created_at) + ")");
            txn.commit();
        });
    }

    void StorageBackendQueries::removeStorageBackend(unsigned int backendId) {
        Transactions::exec("StorageBackendQueries::removeStorageBackend", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM storage_backends WHERE id = " + txn.quote(backendId));
            txn.commit();
        });
    }

    std::vector<vh::types::StorageBackend> StorageBackendQueries::listStorageBackends() {
        return Transactions::exec("StorageBackendQueries::listStorageBackends", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM storage_backends");
            std::vector<vh::types::StorageBackend> backends;
            for (const auto& row : res) backends.emplace_back(row);
            return backends;
        });
    }

    vh::types::StorageBackend StorageBackendQueries::getStorageBackend(unsigned int backendId) {
        return Transactions::exec("StorageBackendQueries::getStorageBackend", [&](pqxx::work& txn) {
            pqxx::result res = txn.exec("SELECT * FROM storage_backends WHERE id = " + txn.quote(backendId));
            if (res.empty()) throw std::runtime_error("No storage backend found with ID: " + std::to_string(backendId));
            return vh::types::StorageBackend(res[0]);
        });
    }
}

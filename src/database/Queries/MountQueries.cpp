#include "database/Queries/MountQueries.hpp"
#include "database/Transactions.hpp"

namespace vh::database {
    void MountQueries::createMount(const std::string& mountName, const std::string& mountType) {
        vh::database::Transactions::exec("MountQueries::createMount", [&](pqxx::work& txn) {
            txn.exec("INSERT INTO mounts (name, type) VALUES (" +
                     txn.quote(mountName) + ", " + txn.quote(mountType) + ")");
        });
    }

    void MountQueries::deleteMount(const std::string& mountName) {
        vh::database::Transactions::exec("MountQueries::deleteMount", [&](pqxx::work& txn) {
            txn.exec("DELETE FROM mounts WHERE name = " + txn.quote(mountName));
        });
    }

    bool MountQueries::mountExists(const std::string& mountName) {
        return vh::database::Transactions::exec("MountQueries::mountExists", [&](pqxx::work& txn) -> bool {
            pqxx::result res = txn.exec("SELECT 1 FROM mounts WHERE name = " + txn.quote(mountName));
            return !res.empty();
        });
    }
}

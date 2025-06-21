#include "database/Queries/FileQueries.hpp"
#include "database/Transactions.hpp"

namespace vh::database {
void FileQueries::createFile(const std::string& fileName, const std::string& mountName, const std::string& content) {
    vh::database::Transactions::exec("FileQueries::createFile", [&](pqxx::work& txn) {
        txn.exec("INSERT INTO files (name, mount_name, content) VALUES (" + txn.quote(fileName) + ", " +
                 txn.quote(mountName) + ", " + txn.quote(content) + ")");
    });
}

void FileQueries::deleteFile(const std::string& fileName, const std::string& mountName) {
    vh::database::Transactions::exec("FileQueries::deleteFile", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM files WHERE name = " + txn.quote(fileName) + " AND mount_name = " + txn.quote(mountName));
    });
}

bool FileQueries::fileMetaExists(const std::string& fileName, const std::string& mountName) {
    return vh::database::Transactions::exec("FileQueries::fileMetaExists", [&](pqxx::work& txn) -> bool {
        pqxx::result res = txn.exec("SELECT 1 FROM files WHERE name = " + txn.quote(fileName) +
                                    " AND mount_name = " + txn.quote(mountName));
        return !res.empty();
    });
}

std::string FileQueries::getFileMeta(const std::string& fileName, const std::string& mountName) {
    return vh::database::Transactions::exec("FileQueries::getFileMeta", [&](pqxx::work& txn) -> std::string {
        pqxx::result res = txn.exec("SELECT content FROM files WHERE name = " + txn.quote(fileName) +
                                    " AND mount_name = " + txn.quote(mountName));
        if (res.empty()) {
            throw std::runtime_error("File not found: " + fileName);
        }
        return res[0][0].as<std::string>();
    });
}
} // namespace vh::database

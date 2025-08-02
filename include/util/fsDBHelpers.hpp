#pragma once

#include <memory>
#include <pqxx/pqxx>

namespace vh::types {
struct File;
struct FSEntry;
}

namespace vh::database {

void updateFile(pqxx::work& txn, const std::shared_ptr<types::File>& file);
void updateFSEntry(pqxx::work& txn, const std::shared_ptr<types::FSEntry>& entry);

}

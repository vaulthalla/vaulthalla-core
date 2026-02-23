#pragma once

#include <memory>
#include <pqxx/pqxx>

namespace vh::fs::model {
struct File;
struct Entry;
}

namespace vh::database {

void updateFile(pqxx::work& txn, const std::shared_ptr<fs::model::File>& file);
void updateFSEntry(pqxx::work& txn, const std::shared_ptr<fs::model::Entry>& entry);

}

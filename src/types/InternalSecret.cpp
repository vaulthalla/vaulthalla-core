#include "types/InternalSecret.hpp"

#include <pqxx/row>
#include <pqxx/binarystring>

using namespace vh::types;

InternalSecret::InternalSecret(const pqxx::row& row)
    : key(row["key"].c_str()),
      created_at(row["created_at"].as<std::time_t>()),
      updated_at(row["updated_at"].as<std::time_t>()) {
    const pqxx::binarystring blob(row["value"]);
    value.assign(blob.begin(), blob.end());

    const pqxx::binarystring iv_blob(row["iv"]);
    iv.assign(iv_blob.begin(), iv_blob.end());
}

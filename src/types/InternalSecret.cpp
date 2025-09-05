#include "types/InternalSecret.hpp"
#include "util/bytea.hpp"

#include <pqxx/row>

using namespace vh::types;
using namespace vh::util;

InternalSecret::InternalSecret(const pqxx::row& row)
    : key(row["key"].c_str()),
      value(from_hex_bytea(row["value"].as<std::string>())),
      iv(from_hex_bytea(row["iv"].as<std::string>())),
      created_at(row["created_at"].as<std::time_t>()),
      updated_at(row["updated_at"].as<std::time_t>()) {}

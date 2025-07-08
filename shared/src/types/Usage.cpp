#include "types/Usage.hpp"
#include "shared_util/timestamp.hpp"

using namespace vh::types;

Usage::Usage(const pqxx::row& row)
    : user_id(row["user_id"].as<unsigned int>()),
      storage_volume_id(row["storage_volume_id"].as<unsigned int>()),
      total_bytes(row["total_bytes"].as<unsigned long long>()),
      used_bytes(row["used_bytes"].as<unsigned long long>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(util::parsePostgresTimestamp(row["updated_at"].as<std::string>())) {}

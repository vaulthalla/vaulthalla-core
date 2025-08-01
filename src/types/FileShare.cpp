#include "types/FileShare.hpp"
#include "util//timestamp.hpp"

using namespace vh::types;

FileShare::FileShare(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      file_acl_id(row["file_acl_id"].as<unsigned int>()),
      shared_by(row["shared_by"].as<unsigned int>()),
      share_token(row["share_token"].as<std::string>()),
      expires_at(row["expires_at"].is_null() ? std::nullopt : std::make_optional(util::parsePostgresTimestamp(row["expires_at"].as<std::string>()))),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())) {}

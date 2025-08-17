#include "types/TrashedFile.hpp"
#include <boost/uuid/nil_generator.hpp>
#include <boost/uuid/string_generator.hpp>

#include "util/timestamp.hpp"
#include <pqxx/result>

using namespace vh::types;

TrashedFile::TrashedFile(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      vault_id(row["vault_id"].as<unsigned int>()),
      base32_alias(row["base32_alias"].as<std::string>()),
      path(row["path"].as<std::string>()),
      backing_path(row["backing_path"].as<std::string>()),
      trashed_at(util::parsePostgresTimestamp(row["trashed_at"].as<std::string>())),
      trashed_by(row["trashed_by"].as<unsigned int>()) {
    if (row["deleted_at"].is_null()) deleted_at = std::nullopt;
    else deleted_at = util::parsePostgresTimestamp(row["deleted_at"].as<std::string>());
}

std::vector<std::shared_ptr<TrashedFile>> vh::types::trashed_files_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<TrashedFile>> files;
    files.reserve(res.size());
    for (const auto& row : res) files.emplace_back(std::make_shared<TrashedFile>(row));
    return files;
}

#include "types/sync/Artifact.hpp"
#include "util/timestamp.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>

using namespace vh::types::sync;
using namespace vh::util;

Artifact::Artifact(const pqxx::row& row)
    : id(row["id"].as<uint32_t>()),
      conflict_id(row["conflict_id"].as<uint32_t>()),
      size_bytes(row["size_bytes"].as<uint64_t>()),
      mime_type(row["mime_type"].as<std::string>()),
      content_hash(row["content_hash"].as<std::string>()),
      last_modified(parsePostgresTimestamp(row["last_modified"].as<std::string>())),
      created_at(parsePostgresTimestamp(row["created_at"].as<std::string>())) {
    parseSide(row["side"].as<std::string>());

    if (row["key_version"].is_null()) key_version = std::nullopt;
    else key_version = row["key_version"].as<uint32_t>();

    if (row["encryption_iv"].is_null()) encryption_iv = std::nullopt;
    else encryption_iv = row["encryption_iv"].as<std::string>();

    if (row["local_backing_path"].is_null()) local_backing_path = std::string();
    else local_backing_path = row["local_backing_path"].as<std::string>();
}

void Artifact::parseSide(const std::string& sideStr) {
    if (sideStr == "local") side = Side::LOCAL;
    else if (sideStr == "upstream" || sideStr == "remote") side = Side::UPSTREAM;
    else throw std::invalid_argument("Invalid side string: " + sideStr);
}

std::string Artifact::sideToString() const {
    switch (side) {
        case Side::LOCAL: return "local";
        case Side::UPSTREAM: return "upstream";
        default: return "unknown";
    }
}

void vh::types::sync::to_json(nlohmann::json& j, const Artifact& artifact) {
    j = {
        {"id", artifact.id},
        {"conflict_id", artifact.conflict_id},
        {"size_bytes", artifact.size_bytes},
        {"mime_type", artifact.mime_type},
        {"content_hash", artifact.content_hash},
        {"side", artifact.sideToString()},
        {"last_modified", timestampToString(artifact.last_modified)},
        {"created_at", timestampToString(artifact.created_at)}
    };

    if (artifact.key_version) j["key_version"] = artifact.key_version.value();
    if (!artifact.local_backing_path.empty()) j["local_backing_path"] = artifact.local_backing_path;
}

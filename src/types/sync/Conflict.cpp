#include "types/sync/Conflict.hpp"
#include "types/fs/File.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/FSCache.hpp"
#include "util/timestamp.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <pqxx/result>

using namespace vh::types::sync;
using namespace vh::services;
using namespace vh::util;

Conflict::Conflict(const pqxx::row& row, const pqxx::result& artifactRows)
    : id(row["id"].as<uint32_t>()),
      event_id(row["event_id"].as<uint32_t>()),
      file(std::static_pointer_cast<File>(ServiceDepsRegistry::instance().fsCache->getEntryById(row["file_id"].as<uint32_t>()))),
      resolved_at(parsePostgresTimestamp(row["resolved_at"].as<std::string>())),
      created_at(parsePostgresTimestamp(row["created_at"].as<std::string>())),
      artifacts(artifactRows) {
    parseType(row["type"].as<std::string>());
    parseResolution(row["resolution"].as<std::string>());
}

std::string Conflict::typeToString() const {
    switch (type) {
        case Type::CONTENT: return "content";
        case Type::METADATA: return "metadata";
        case Type::ENCRYPTION: return "encryption";
    }

    return "unknown";
}

std::string Conflict::resolutionToString() const {
    switch (resolution) {
        case Resolution::UNRESOLVED: return "unresolved";
        case Resolution::KEPT_LOCAL: return "kept_local";
        case Resolution::KEPT_REMOTE: return "kept_remote";
        case Resolution::FIXED_REMOTE_ENCRYPTION: return "fixed_remote_encryption";
    }

    return "unknown";
}

void Conflict::parseResolution(const std::string& resolutionStr) {
    if (resolutionStr == "unresolved") resolution = Resolution::UNRESOLVED;
    else if (resolutionStr == "kept_local") resolution = Resolution::KEPT_LOCAL;
    else if (resolutionStr == "kept_remote") resolution = Resolution::KEPT_REMOTE;
    else if (resolutionStr == "fixed_remote_encryption") resolution = Resolution::FIXED_REMOTE_ENCRYPTION;
    else resolution = Resolution::UNRESOLVED; // Default to unresolved if unknown
}

void Conflict::parseType(const std::string& typeStr) {
    if (typeStr == "content") type = Type::CONTENT;
    else if (typeStr == "metadata") type = Type::METADATA;
    else if (typeStr == "encryption") type = Type::ENCRYPTION;
    else type = Type::CONTENT; // Default to content if unknown
}

void vh::types::sync::to_json(nlohmann::json& j, const std::shared_ptr<Conflict>& conflict) {
    j = {
        {"id", conflict->id},
        {"event_id", conflict->event_id},
        {"file", *conflict->file},
        {"type", conflict->typeToString()},
        {"resolution", conflict->resolutionToString()},
        {"resolved_at", conflict->resolved_at},
        {"created_at", conflict->created_at},
        {"artifacts", conflict->artifacts}
    };
}


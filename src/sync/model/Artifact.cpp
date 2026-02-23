#include "sync/model/Artifact.hpp"
#include "fs/model/File.hpp"
#include "util/timestamp.hpp"
#include "services/ServiceDepsRegistry.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>

using namespace vh::sync::model;
using namespace vh::fs::model;
using namespace vh::util;
using namespace vh::services;

Artifact::Artifact(const pqxx::row& row)
    : id(row["id"].as<uint32_t>()),
      conflict_id(row["conflict_id"].as<uint32_t>()),
      file(std::static_pointer_cast<File>(ServiceDepsRegistry::instance().fsCache->getEntryById(row["file_id"].as<uint32_t>()))) {
    parseSide(row["side"].as<std::string>());
}

Artifact::Artifact(const std::shared_ptr<File>& f, const Side& s) {
    file = f;
    side = s;
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

void vh::sync::model::to_json(nlohmann::json& j, const Artifact& artifact) {
    j = {
        {"id", artifact.id},
        {"conflict_id", artifact.conflict_id},
        {"side", artifact.sideToString()},
        {"file", *artifact.file}
    };
}

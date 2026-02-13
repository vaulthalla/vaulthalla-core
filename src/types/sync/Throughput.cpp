#include "types/sync/Throughput.hpp"
#include "util/timestamp.hpp"

#include <chrono>
#include <pqxx/row>
#include <nlohmann/json.hpp>

using namespace vh::types::sync;
using namespace std::chrono;
using namespace vh::util;

Throughput::Throughput(const pqxx::row& row) :
    id(row["id"].as<uint32_t>()),
    sync_event_id(row["sync_event_id"].as<uint32_t>()),
    num_ops(row["num_ops"].as<uint64_t>()),
    size_bytes(row["size_bytes"].as<uint64_t>()),
    timestamp_begin(parsePostgresTimestamp(row["timestamp_begin"].as<std::string>())),
    timestamp_end(parsePostgresTimestamp(row["timestamp_end"].as<std::string>())) {}

void Throughput::start() { timestamp_begin = system_clock::to_time_t(system_clock::now()); }
void Throughput::stop() { timestamp_end = system_clock::to_time_t(system_clock::now()); }

void Throughput::parseMetric(const std::string& str) {
    if (str == "rename") metric_type = RENAME;
    else if (str == "copy") metric_type = COPY;
    else if (str == "delete") metric_type = DELETE;
    else if (str == "upload") metric_type = UPLOAD;
    else if (str == "download") metric_type = DOWNLOAD;
}

std::string Throughput::metricToString() const {
    switch (metric_type) {
        case RENAME: return "rename";
        case COPY: return "copy";
        case DELETE: return "delete";
        case UPLOAD: return "upload";
        case DOWNLOAD: return "download";
        default: return "unknown";
    }
}

void vh::types::sync::to_json(nlohmann::json& j, const Throughput& t) {
    j = {
        {"id", t.id},
        {"sync_event_id", t.sync_event_id},
        {"num_ops", t.num_ops},
        {"size_bytes", t.size_bytes},
        {"timestamp_begin", t.timestamp_begin},
        {"timestamp_end", t.timestamp_end},
        {"metric_type", t.metricToString()}
    };
}

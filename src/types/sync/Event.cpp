#include "types/sync/Event.hpp"
#include "util/timestamp.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <pqxx/row>
#include <nlohmann/json.hpp>

namespace {
    std::string lowerCopy(std::string_view sv) {
        std::string s(sv);
        std::ranges::transform(s.begin(), s.end(), s.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    // Safer helper: returns 0 when NULL.
    template <typename T>
    T as_or_default(const pqxx::row& r, const char* col, T def) {
        const auto f = r[col];
        return f.is_null() ? def : f.as<T>();
    }

    // Safer helper: returns empty string when NULL.
    std::string as_or_empty(const pqxx::row& r, const char* col) {
        const auto f = r[col];
        return f.is_null() ? std::string{} : f.as<std::string>();
    }
}

using namespace vh::types::sync;
using namespace vh::util;

Event::Event(const pqxx::row& row)
    : id(row["id"].as<uint32_t>())
    , vault_id(row["vault_id"].as<uint32_t>())
    , sync_id(row["sync_id"].as<uint32_t>())
    , timestamp_begin(parsePostgresTimestamp(row["timestamp_begin"].as<std::string>()))
    , timestamp_end(row["timestamp_end"].is_null()
        ? 0
        : parsePostgresTimestamp(row["timestamp_end"].as<std::string>()))
    , heartbeat_at(row["heartbeat_at"].is_null()
        ? 0
        : parsePostgresTimestamp(row["heartbeat_at"].as<std::string>()))
    , retry_attempt(as_or_default<uint32_t>(row, "retry_attempt", 0))
    , stall_reason(as_or_empty(row, "stall_reason"))
    , error_code(as_or_empty(row, "error_code"))
    , error_message(as_or_empty(row, "error_message"))
    , num_ops_total(as_or_default<std::uint64_t>(row, "num_ops_total", 0))
    , num_failed_ops(as_or_default<std::uint64_t>(row, "num_failed_ops", 0))
    , num_conflicts(as_or_default<std::uint64_t>(row, "num_conflicts", 0))
    , bytes_up(as_or_default<std::uint64_t>(row, "bytes_up", 0))
    , bytes_down(as_or_default<std::uint64_t>(row, "bytes_down", 0))
    , divergence_detected(as_or_default<bool>(row, "divergence_detected", false))
    , local_change_seq_begin(as_or_default<std::int64_t>(row, "local_change_seq_begin", -1))
    , local_change_seq_end(as_or_default<std::int64_t>(row, "local_change_seq_end", -1))
    , remote_change_seq_begin(as_or_default<std::int64_t>(row, "remote_change_seq_begin", -1))
    , remote_change_seq_end(as_or_default<std::int64_t>(row, "remote_change_seq_end", -1))
    , local_state_hash(as_or_empty(row, "local_state_hash"))
    , remote_state_hash(as_or_empty(row, "remote_state_hash"))
    , worker_id(as_or_empty(row, "worker_id"))
    , build_version(as_or_empty(row, "build_version"))
    , git_sha(as_or_empty(row, "git_sha"))
    , config_hash(as_or_empty(row, "config_hash"))
{
    // Parse status + trigger with sane fallbacks
    {
        State s = State::RUNNING;
        if (tryParseState(as_or_empty(row, "status"), s)) state = s;
    }
    {
        Trigger t = Trigger::SCHEDULE;
        if (tryParseTrigger(as_or_empty(row, "trigger"), t)) trigger = t;
    }
}

void Event::addThroughput(std::unique_ptr<Throughput> t) {
    if (!t) return;
    throughputs.emplace_back(std::move(t));
}

const Throughput* Event::getThroughput(const Throughput::Metric& metric) const noexcept {
    const auto m = static_cast<Throughput::Metric>(metric);
    for (const auto& p : throughputs) {
        if (p && p->metric_type == m) return p.get();
    }
    return nullptr;
}

Throughput* Event::getThroughput(const Throughput::Metric& metric) noexcept {
    const auto m = static_cast<Throughput::Metric>(metric);
    for (auto& p : throughputs) if (p && p->metric_type == m) return p.get();
    return nullptr;
}

Throughput& Event::getOrCreateThroughput(const Throughput::Metric& metric) {
    if (auto* existing = getThroughput(metric)) return *existing;

    auto created = std::make_unique<Throughput>();
    created->sync_event_id = id;
    created->metric_type = static_cast<Throughput::Metric>(metric);

    throughputs.emplace_back(std::move(created));
    return *throughputs.back();
}

void Event::computeDashboardStats() {
    // Reset derived values (leave num_failed_ops/num_conflicts as they may be tracked elsewhere too)
    num_ops_total = 0;
    bytes_up = 0;
    bytes_down = 0;

    for (const auto& p : throughputs) {
        if (!p) continue;

        num_ops_total += p->num_ops;

        switch (p->metric_type) {
            case Throughput::UPLOAD:
                bytes_up += p->size_bytes;
                break;
            case Throughput::DOWNLOAD:
                bytes_down += p->size_bytes;
                break;
            default:
                break;
        }
    }
}

// -------------------------
// Enum ↔ string
// -------------------------
std::string_view Event::toString(State s) noexcept {
    switch (s) {
        case State::RUNNING:   return "running";
        case State::SUCCESS:   return "success";
        case State::STALLED:   return "stalled";
        case State::ERROR:     return "error";
        case State::CANCELLED: return "cancelled";
        default:               return "running";
    }
}

std::string_view Event::toString(Trigger t) noexcept {
    switch (t) {
        case Trigger::SCHEDULE: return "schedule";
        case Trigger::MANUAL:   return "manual";
        case Trigger::STARTUP:  return "startup";
        case Trigger::WEBHOOK:  return "webhook";
        case Trigger::RETRY:    return "retry";
        default:                return "schedule";
    }
}

bool Event::tryParseState(std::string_view in, State& out) noexcept {
    const std::string s = lowerCopy(in);
    if (s == "running")   { out = State::RUNNING; return true; }
    if (s == "success")   { out = State::SUCCESS; return true; }
    if (s == "stalled")   { out = State::STALLED; return true; }
    if (s == "error")     { out = State::ERROR; return true; }
    if (s == "cancelled") { out = State::CANCELLED; return true; }
    if (s == "canceled")  { out = State::CANCELLED; return true; }
    return false;
}

bool Event::tryParseTrigger(std::string_view in, Trigger& out) noexcept {
    const std::string s = lowerCopy(in);
    if (s == "schedule" || s == "scheduled") { out = Trigger::SCHEDULE; return true; }
    if (s == "manual")                       { out = Trigger::MANUAL; return true; }
    if (s == "startup")                      { out = Trigger::STARTUP; return true; }
    if (s == "webhook")                      { out = Trigger::WEBHOOK; return true; }
    if (s == "retry")                        { out = Trigger::RETRY; return true; }
    return false;
}

// -------------------------
// JSON
// -------------------------
void vh::types::sync::to_json(nlohmann::json& j, const Event& e) {
    j = {
        {"id", e.id},
        {"vault_id", e.vault_id},
        {"sync_id", e.sync_id},

        {"timestamp_begin", e.timestamp_begin},
        {"timestamp_end", e.timestamp_end},
        {"heartbeat_at", e.heartbeat_at},

        {"status", std::string(Event::toString(e.state))},
        {"trigger", std::string(Event::toString(e.trigger))},
        {"retry_attempt", e.retry_attempt},

        {"stall_reason", e.stall_reason},
        {"error_code", e.error_code},
        {"error_message", e.error_message},

        {"num_ops_total", e.num_ops_total},
        {"num_failed_ops", e.num_failed_ops},
        {"num_conflicts", e.num_conflicts},
        {"bytes_up", e.bytes_up},
        {"bytes_down", e.bytes_down},

        {"divergence_detected", e.divergence_detected},
        {"local_change_seq_begin", e.local_change_seq_begin},
        {"local_change_seq_end", e.local_change_seq_end},
        {"remote_change_seq_begin", e.remote_change_seq_begin},
        {"remote_change_seq_end", e.remote_change_seq_end},
        {"local_state_hash", e.local_state_hash},
        {"remote_state_hash", e.remote_state_hash},

        {"worker_id", e.worker_id},
        {"build_version", e.build_version},
        {"git_sha", e.git_sha},
        {"config_hash", e.config_hash},
    };

    // Include throughputs (as array of objects)
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& p : e.throughputs) {
        if (!p) continue;
        nlohmann::json tj;
        vh::types::sync::to_json(tj, *p);
        arr.push_back(std::move(tj));
    }
    j["throughputs"] = std::move(arr);
}

#pragma once

#include "types/sync/Throughput.hpp"

#include <ctime>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

namespace pqxx { class row; }

namespace vh::types::sync {

struct Event {
    // Mirrors DB values:
    // status: running/success/stalled/error/cancelled
    // trigger: schedule/manual/startup/webhook/retry

    enum class State : uint8_t {
        RUNNING,
        SUCCESS,
        STALLED,
        ERROR,
        CANCELLED
    };

    enum class Trigger : uint8_t {
        SCHEDULE,
        MANUAL,
        STARTUP,
        WEBHOOK,
        RETRY
    };

    // Core identifiers
    uint32_t id{0};
    uint32_t vault_id{0};
    uint32_t sync_id{0};

    // Timing (0 means NULL / not set)
    std::time_t timestamp_begin{0};
    std::time_t timestamp_end{0};
    std::time_t heartbeat_at{0};

    // Run metadata
    State state{State::RUNNING};
    Trigger trigger{Trigger::SCHEDULE};
    uint32_t retry_attempt{0};

    // Diagnostics
    std::string stall_reason;   // optional
    std::string error_code;     // optional stable programmatic identifier
    std::string error_message;  // optional human readable

    // Detailed metrics
    std::vector<std::unique_ptr<Throughput>> throughputs;

    // Summary counters (dashboard-friendly; derived from throughputs via computeDashboardStats())
    std::uint64_t num_ops_total{0};
    std::uint64_t num_failed_ops{0};
    std::uint64_t num_conflicts{0};
    std::uint64_t bytes_up{0};
    std::uint64_t bytes_down{0};

    // Divergence / watermarks
    bool divergence_detected{false};
    std::string local_state_hash;   // optional
    std::string remote_state_hash;  // optional

    // Attribution (multi-worker debugging)
    std::string config_hash;

    Event() = default;
    explicit Event(const pqxx::row& row);

    // -------------------------
    // Convenience helpers
    // -------------------------
    void start();
    void stop();

    [[nodiscard]] bool hasEnded() const noexcept { return timestamp_end != 0; }
    [[nodiscard]] bool hasHeartbeat() const noexcept { return heartbeat_at != 0; }

    [[nodiscard]] std::time_t durationSeconds() const noexcept {
        if (timestamp_begin == 0) return 0;
        const std::time_t end = (timestamp_end != 0) ? timestamp_end : std::time(nullptr);
        return (end >= timestamp_begin) ? (end - timestamp_begin) : 0;
    }

    // "Stalled" heuristic: running + no heartbeat for stall_after_seconds
    [[nodiscard]] bool looksStalled(std::time_t now,
                                    std::time_t stall_after_seconds) const noexcept {
        if (state != State::RUNNING) return false;
        if (heartbeat_at == 0) return false;
        return (now > heartbeat_at) && ((now - heartbeat_at) >= stall_after_seconds);
    }

    // -------------------------
    // Throughput integration
    // -------------------------
    void addThroughput(std::unique_ptr<Throughput> t);

    // Find existing throughput bucket by metric (const/non-const)
    [[nodiscard]] const Throughput* getThroughput(const Throughput::Metric& metric) const noexcept;
    [[nodiscard]] Throughput* getThroughput(const Throughput::Metric& metric) noexcept;

    // Creates bucket if missing (uses Throughput::Metric)
    Throughput& getOrCreateThroughput(const Throughput::Metric& metric);

    // Recompute summary fields from throughputs.
    // Call at end-of-run (and optionally periodically for live dashboards).
    void computeDashboardStats();

    // -------------------------
    // Enum ↔ string
    // -------------------------
    static std::string_view toString(State s) noexcept;
    static std::string_view toString(Trigger t) noexcept;

    // Returns false if unrecognized (and leaves out unchanged)
    static bool tryParseState(std::string_view in, State& out) noexcept;
    static bool tryParseTrigger(std::string_view in, Trigger& out) noexcept;
};

void to_json(nlohmann::json& j, const Event& e);

}

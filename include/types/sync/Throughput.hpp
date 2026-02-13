#pragma once

#include <ctime>
#include <cstdint>
#include <string>
#include <nlohmann/json_fwd.hpp>

namespace pqxx { class row; }

namespace vh::types::sync {

struct Throughput {
    enum Metric {
        UPLOAD,
        DOWNLOAD,
        RENAME,
        COPY,
        DELETE
    };

    uint32_t id{};
    uint32_t sync_event_id{};

    Metric metric_type{RENAME};

    uint64_t num_ops{};
    uint64_t size_bytes{};

    std::time_t timestamp_begin{};
    std::time_t timestamp_end{};

    Throughput() = default;
    explicit Throughput(const pqxx::row& row);

    void start();
    void stop();

    void parseMetric(const std::string& str);
    std::string metricToString() const;
};

void to_json(nlohmann::json& j, const Throughput& t);

}

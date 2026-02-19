# pragma once

#include <memory>
#include <string>
#include <optional>
#include <nlohmann/json_fwd.hpp>
#include <filesystem>

namespace pqxx {
    class row;
}

namespace vh::types::sync {

struct Artifact {
    enum class Side { LOCAL, UPSTREAM };

    uint32_t id{}, conflict_id{};
    std::optional<std::string> encryption_iv{};
    std::optional<uint32_t> key_version{};
    uint64_t size_bytes{};
    Side side{};
    std::string mime_type{}, content_hash{};
    std::time_t last_modified{}, created_at{};
    std::filesystem::path local_backing_path{};

    Artifact() = default;
    explicit Artifact(const pqxx::row& row);

    std::string sideToString() const;
    void parseSide(const std::string& sideStr);
};

void to_json(nlohmann::json& j, const Artifact& artifact);

}

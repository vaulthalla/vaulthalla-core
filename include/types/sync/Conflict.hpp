#pragma once

#include "ConflictArtifact.hpp"

#include <string>
#include <ctime>
#include <memory>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
    class row;
    class result;
}

namespace vh::types {
struct File;
}

namespace vh::types::sync {

struct Conflict {
    enum class Type { CONTENT, METADATA, ENCRYPTION };
    enum class Resolution { UNRESOLVED, KEPT_LOCAL, KEPT_REMOTE, FIXED_REMOTE_ENCRYPTION };

    uint32_t id{}, event_id{};
    std::shared_ptr<File> file{};
    std::time_t resolved_at{}, created_at{};

    Type type{Type::CONTENT};
    Resolution resolution{Resolution::UNRESOLVED};

    ConflictArtifact artifacts{};

    Conflict() = default;
    explicit Conflict(const pqxx::row& row, const pqxx::result& artifactRows);

    [[nodiscard]] std::string typeToString() const;
    [[nodiscard]] std::string resolutionToString() const;

    void parseType(const std::string& typeStr);
    void parseResolution(const std::string& resolutionStr);
};

void to_json(nlohmann::json& j, const std::shared_ptr<Conflict>& conflict);

}

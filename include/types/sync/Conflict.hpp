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

    struct Reason {
        std::string code{}, message{};

        Reason() = default;
        Reason(std::string c, std::string m) : code{std::move(c)}, message{std::move(m)} {}
    };

    enum class Type { MISMATCH, ENCRYPTION, BOTH };
    enum class Resolution { UNRESOLVED, KEPT_LOCAL, KEPT_REMOTE, KEPT_BOTH, OVERWRITTEN, FIXED_REMOTE_ENCRYPTION };

    uint32_t id{}, event_id{}, file_id{};
    std::time_t resolved_at{}, created_at{};

    Type type{Type::MISMATCH};
    Resolution resolution{Resolution::UNRESOLVED};

    ConflictArtifact artifacts{};
    std::vector<Reason> reasons{};

    // not db-backed, used to indicate that the conflict was caused by a failed decryption of an upstream file
    bool failed_to_decrypt_upstream{};

    Conflict() = default;
    explicit Conflict(const pqxx::row& row, const pqxx::result& artifactRows);

    [[nodiscard]] std::string typeToString() const;
    [[nodiscard]] std::string resolutionToString() const;

    void parseType(const std::string& typeStr);
    void parseResolution(const std::string& resolutionStr);
    void analyze();
};

void to_json(nlohmann::json& j, const std::shared_ptr<Conflict>& conflict);
void to_json(nlohmann::json& j, const Conflict::Reason& reason);
void to_json(nlohmann::json& j, const std::vector<Conflict::Reason>& reasons);

}

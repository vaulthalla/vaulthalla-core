#pragma once

#include <ctime>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <filesystem>

namespace pqxx {
    class row;
}

namespace vh::types {

struct Volume {
    unsigned int id{0};
    unsigned int vault_id{0};
    std::string name;
    std::filesystem::path path_prefix;
    std::optional<unsigned long long> quota_bytes;
    std::time_t created_at{std::time(nullptr)};

    Volume() = default;
    explicit Volume(const pqxx::row& row);
    Volume(unsigned int vaultId, std::string name, std::filesystem::path pathPrefix,
                  std::optional<unsigned long long> quotaBytes = std::nullopt);
};

void to_json(nlohmann::json& j, const Volume& v);
void from_json(const nlohmann::json& j, Volume& v);
void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<Volume>>& volumes);

} // namespace vh::types

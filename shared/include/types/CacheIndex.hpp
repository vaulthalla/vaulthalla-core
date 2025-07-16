#pragma once

#include <filesystem>
#include <ctime>
#include <memory>
#include <vector>
#include <nlohmann/json_fwd.hpp>
#include <string>

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct CacheIndex {
    enum class Type { File, Thumbnail };

    unsigned int id{}, vault_id{}, file_id{};
    std::filesystem::path path{};
    Type type{Type::Thumbnail};
    uintmax_t size{};
    std::time_t last_accessed{}, created_at{};

    CacheIndex() = default;
    explicit CacheIndex(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const CacheIndex& index);
void from_json(const nlohmann::json& j, CacheIndex& index);

std::string to_string(const CacheIndex::Type& type);
CacheIndex::Type typeFromString(const std::string& str);

std::vector<std::shared_ptr<CacheIndex>> cache_indices_from_pq_res(const pqxx::result& res);

}

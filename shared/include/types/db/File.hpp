#pragma once

#include <ctime>
#include <optional>
#include <string>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
    class row;
}

namespace vh::types {

struct File {
    unsigned int id;
    unsigned int storage_volume_id;
    std::optional<unsigned int> parent_id;
    std::string name;
    bool is_directory = false;
    unsigned long long mode;
    unsigned int uid;
    unsigned int gid;
    unsigned int created_by;
    std::time_t created_at;
    std::time_t updated_at;
    unsigned long long current_version_size_bytes;
    bool is_trashed;
    std::time_t trashed_at;
    unsigned int trashed_by;
    std::optional<std::string> full_path;

    File() = default;
    explicit File(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const File& f);
void from_json(const nlohmann::json& j, File& f);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<File>>& files);

} // namespace vh::types

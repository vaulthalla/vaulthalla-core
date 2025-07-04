#pragma once

#include <filesystem>
#include <string>
#include <ctime>
#include <optional>
#include <nlohmann/json_fwd.hpp>

namespace pqxx {
class row;
class result;
}

namespace vh::types {

struct File;
struct Directory;

struct FSEntry {
    unsigned int id{};
    unsigned int vault_id{}, created_by{}, last_modified_by{};
    std::optional<unsigned int> parent_id;
    std::string name{};
    std::time_t created_at{}, updated_at{};
    std::filesystem::path path{};

    FSEntry() = default;
    virtual ~FSEntry() = default;

    [[nodiscard]] virtual bool isDirectory() const = 0;

    explicit FSEntry(const pqxx::row& row);
};

void to_json(nlohmann::json& j, const FSEntry& entry);

void from_json(const nlohmann::json& j, FSEntry& entry);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<FSEntry>>& entries);

std::vector<std::shared_ptr<FSEntry> > merge_entries(const std::vector<std::shared_ptr<File>>& files,
                                                     const std::vector<std::shared_ptr<Directory>>& directories);

}
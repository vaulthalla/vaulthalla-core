#pragma once

#include "types/FSEntry.hpp"
#include <vector>

namespace vh::types {

struct Directory : FSEntry {
    unsigned int file_count{0};
    unsigned int subdirectory_count{0};
    std::time_t last_modified{0};

    Directory() = default;
    explicit Directory(const pqxx::row& row);
    [[nodiscard]] bool isDirectory() const override { return true; }
};

void to_json(nlohmann::json& j, const Directory& d);
void from_json(const nlohmann::json& j, Directory& d);

std::vector<std::shared_ptr<Directory>> directories_from_pq_res(const pqxx::result& res);

}

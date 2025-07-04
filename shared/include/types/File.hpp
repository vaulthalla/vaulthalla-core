#pragma once

#include "types/FSEntry.hpp"

namespace vh::types {

struct File : FSEntry {
    unsigned long long size_bytes{};
    std::optional<std::string> mime_type, content_hash;

    File() = default;
    explicit File(const pqxx::row& row);
    [[nodiscard]] bool isDirectory() const override { return false; }
};

void to_json(nlohmann::json& j, const File& f);
void from_json(const nlohmann::json& j, File& f);

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<File>>& files);

std::vector<std::shared_ptr<File>> files_from_pq_res(const pqxx::result& res);

} // namespace vh::types

#include "types/File.hpp"
#include "shared_util/timestamp.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <pqxx/result>
#include <format>

namespace vh::types {

inline std::string octMode(const unsigned long long mode) {
    return std::format("{:04o}", mode & 07777);   // -> "0644"
}

inline std::string hexMode(const unsigned long long mode) {
    return std::format("0x{:03X}", mode & 0xFFF); // -> "0x1A4"
}

File::File(const pqxx::row& row)
    : FSEntry(row), size_bytes(row.at("size_bytes").as<unsigned long long>()) {

    if (row.at("mime_type").is_null()) mime_type = std::nullopt;
    else mime_type = row.at("mime_type").as<std::string>();

    if (row.at("content_hash").is_null()) content_hash = std::nullopt;
    else content_hash = row.at("content_hash").as<std::string>();
}

void to_json(nlohmann::json& j, const File& f) {
    to_json(j, static_cast<const FSEntry&>(f));
    j["size_bytes"] = f.size_bytes;
    if (f.mime_type.has_value()) j["mime_type"] = *f.mime_type;
    if (f.content_hash.has_value()) j["content_hash"] = *f.content_hash;
    j["type"] = "file"; // Helpful for client
}

void from_json(const nlohmann::json& j, File& f) {
    from_json(j, static_cast<FSEntry&>(f));

    f.size_bytes = j.at("size_bytes").get<unsigned long long>();

    if (j.contains("mime_type") && !j["mime_type"].is_null()) f.mime_type = j.at("mime_type").get<std::string>();
    else f.mime_type.reset();

    if (j.contains("content_hash") && !j["content_hash"].is_null()) f.content_hash = j.at("content_hash").get<std::string>();
    else f.content_hash.reset();
}

void to_json(nlohmann::json& j, const std::vector<std::shared_ptr<File>>& files) {
    j = nlohmann::json::array();
    for (const auto& file : files) j.push_back(*file);
}

std::vector<std::shared_ptr<File>> files_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<File>> files;
    for (const auto& row : res) files.push_back(std::make_shared<File>(row));
    return files;
}

}

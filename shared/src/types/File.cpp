#include "types/File.hpp"
#include "shared_util/timestamp.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/result>
#include <format>
#include <regex>
#include <pugixml.hpp>
#include <iostream>

using namespace vh::types;

inline std::string octMode(const unsigned long long mode) {
    return std::format("{:04o}", mode & 07777);   // -> "0644"
}

inline std::string hexMode(const unsigned long long mode) {
    return std::format("0x{:03X}", mode & 0xFFF); // -> "0x1A4"
}

File::File(const pqxx::row& row)
    : FSEntry(row), size_bytes(row.at("size_bytes").as<unsigned long long>()),
      encryption_iv(row.at("encryption_iv").as<std::string>()),
      mime_type(row.at("mime_type").as<std::optional<std::string>>()),
      content_hash(row.at("content_hash").as<std::optional<std::string>>()) {}

File::File(const std::string& s3_key, const uint64_t size, const std::optional<std::time_t>& updated)
    : FSEntry(s3_key), size_bytes(size) {
    if (updated) updated_at = *updated;
}

void vh::types::to_json(nlohmann::json& j, const File& f) {
    to_json(j, static_cast<const FSEntry&>(f));
    j["size_bytes"] = f.size_bytes;
    j["type"] = "file"; // Helpful for client

    if (f.mime_type) j["mime_type"] = f.mime_type.value();
    else j["mime_type"] = nullptr;
}

void vh::types::from_json(const nlohmann::json& j, File& f) {
    from_json(j, static_cast<FSEntry&>(f));
    f.size_bytes = j.at("size_bytes").get<unsigned long long>();

    if (j.contains("mime_type")) f.mime_type = j.at("mime_type").get<std::string>();
    else f.mime_type = std::nullopt;
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<File>>& files) {
    j = nlohmann::json::array();
    for (const auto& file : files) j.push_back(*file);
}

std::vector<std::shared_ptr<File>> vh::types::files_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<File>> files;
    for (const auto& row : res) files.push_back(std::make_shared<File>(row));
    return files;
}

std::vector<std::shared_ptr<File>> vh::types::filesFromS3XML(const std::u8string& xml) {
    std::vector<std::shared_ptr<File>> files;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(reinterpret_cast<const char*>(xml.c_str()));

    if (!result) {
        std::cerr << "[fileListFromS3XML] Failed to parse XML: " << result.description() << std::endl;
        return {};
    }

    pugi::xml_node root = doc.child("ListBucketResult");
    if (!root) {
        std::cerr << "[fileListFromS3XML] Missing <ListBucketResult> root node" << std::endl;
        return {};
    }

    for (pugi::xml_node content : root.children("Contents")) {
        auto keyNode = content.child("Key");
        auto sizeNode = content.child("Size");
        auto modifiedNode = content.child("LastModified");

        if (!keyNode || !sizeNode || !modifiedNode) {
            std::cerr << "[fileListFromS3XML] Skipping entry due to missing child elements" << std::endl;
            continue;
        }

        const std::u8string key = reinterpret_cast<const char8_t*>(keyNode.text().get());
        const std::string lastMod = modifiedNode.text().get();
        const uint64_t size = std::stoull(sizeNode.text().get());

        std::tm tm{};
        std::istringstream ss(lastMod);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        std::time_t ts = ss.fail() ? std::time(nullptr) : timegm(&tm);

        auto file = std::make_shared<File>(std::string(reinterpret_cast<const char*>(key.c_str())), size, ts);
        files.push_back(file);
    }

    return files;
}

std::unordered_map<std::u8string, std::shared_ptr<File>> vh::types::groupEntriesByPath(const std::vector<std::shared_ptr<File>>& entries) {
    std::unordered_map<std::u8string, std::shared_ptr<File>> grouped;

    for (const auto& file : entries) {
        if (grouped.contains(file->path.u8string())) {
            std::cerr << "[groupEntriesByPath] Duplicate entry found for path: " << file->path.string() << std::endl;
            continue;
        }
        grouped[file->path.u8string()] = file;
    }

    return grouped;
}

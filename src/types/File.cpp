#include "types/File.hpp"
#include "util/timestamp.hpp"
#include "services/LogRegistry.hpp"
#include "database/Queries/FSEntryQueries.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/result>
#include <regex>
#include <pugixml.hpp>

using namespace vh::types;
using namespace vh::logging;
using namespace vh::database;

File::File(const pqxx::row& row, const pqxx::result& parentRows)
    : FSEntry(row, parentRows),
      encryption_iv(row.at("encryption_iv").as<std::string>()),
      mime_type(row.at("mime_type").as<std::optional<std::string>>()),
      content_hash(row.at("content_hash").as<std::optional<std::string>>()),
      encrypted_with_key_version(row.at("encrypted_with_key_version").as<unsigned int>()) {}

File::File(const std::string& s3_key, const uint64_t size, const std::optional<std::time_t>& updated)
    : FSEntry(s3_key) {
    if (updated) updated_at = *updated;
}

void vh::types::to_json(nlohmann::json& j, const File& f) {
    to_json(j, static_cast<const FSEntry&>(f));
    j["type"] = "file"; // Helpful for client

    if (f.mime_type) j["mime_type"] = f.mime_type.value();
    else j["mime_type"] = nullptr;
}

void vh::types::from_json(const nlohmann::json& j, File& f) {
    from_json(j, static_cast<FSEntry&>(f));

    if (j.contains("mime_type")) f.mime_type = j.at("mime_type").get<std::string>();
    else f.mime_type = std::nullopt;
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<File>>& files) {
    j = nlohmann::json::array();
    for (const auto& file : files) j.push_back(*file);
}

std::vector<std::shared_ptr<File>> vh::types::files_from_pq_res(const pqxx::result& res) {
    std::vector<std::shared_ptr<File>> files;
    for (const auto& row : res) {
        if (const auto parentId = row["parent_id"].as<std::optional<unsigned int>>()) {
            const auto parentChain = FSEntryQueries::collectParentChain(*parentId);
            files.push_back(std::make_shared<File>(row, parentChain));
        } else files.push_back(std::make_shared<File>(row, pqxx::result{}));
    }
    return files;
}

std::vector<std::shared_ptr<File>> vh::types::filesFromS3XML(const std::u8string& xml) {
    std::vector<std::shared_ptr<File>> files;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(reinterpret_cast<const char*>(xml.c_str()));

    if (!result) {
        LogRegistry::types()->error("[File] [filesFromS3XML] Failed to parse XML: {}", result.description());
        return {};
    }

    pugi::xml_node root = doc.child("ListBucketResult");
    if (!root) {
        LogRegistry::types()->error("[File] [filesFromS3XML] No ListBucketResult node found in XML");
        return {};
    }

    for (pugi::xml_node content : root.children("Contents")) {
        auto keyNode = content.child("Key");
        auto sizeNode = content.child("Size");
        auto modifiedNode = content.child("LastModified");

        if (!keyNode || !sizeNode || !modifiedNode) {
            LogRegistry::types()->warn("[File] [filesFromS3XML] Skipping entry due to missing child elements");
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
            LogRegistry::types()->warn("[File] [groupEntriesByPath] Duplicate entry found for path: {}", file->path.string());
            continue;
        }
        grouped[file->path.u8string()] = file;
    }

    return grouped;
}

#include "types/FSEntry.hpp"
#include "util/timestamp.hpp"
#include "types/Directory.hpp"
#include "types/File.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <pugixml.hpp>
#include <iostream>

using namespace vh::types;

FSEntry::FSEntry(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      vault_id(row["vault_id"].as<unsigned int>()),
      created_by(row["created_by"].as<unsigned int>()),
      last_modified_by(row["last_modified_by"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      size_bytes(row["size_bytes"].as<uintmax_t>()),
      created_at(util::parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(util::parsePostgresTimestamp(row["updated_at"].as<std::string>())),
      path(std::filesystem::path(row["path"].as<std::string>())) {
    if (row["parent_id"].is_null()) parent_id = std::nullopt;
    else parent_id = row["parent_id"].as<unsigned int>();
}

FSEntry::FSEntry(const std::string& s3_key) {
    name = std::filesystem::path(s3_key).filename().string();
    path = s3_key;
    created_at = updated_at = std::time(nullptr); // default timestamp (can override)
}

void vh::types::to_json(nlohmann::json& j, const FSEntry& entry) {
    j = {
        {"id", entry.id},
        {"vault_id", entry.vault_id},
        {"created_by", entry.created_by},
        {"last_modified_by", entry.last_modified_by},
        {"name", entry.name},
        {"parent_id", entry.parent_id.has_value() ? entry.parent_id.value() : 0},
        {"size_bytes", entry.size_bytes},
        {"created_at", util::timestampToString(entry.created_at)},
        {"updated_at", util::timestampToString(entry.updated_at)},
        {"path", entry.path.string()}
    };
}

void vh::types::from_json(const nlohmann::json& j, FSEntry& entry) {
    entry.id = j.at("id").get<unsigned int>();
    entry.vault_id = j.at("vault_id").get<unsigned int>();
    entry.created_by = j.at("created_by").get<unsigned int>();
    entry.last_modified_by = j.at("last_modified_by").get<unsigned int>();
    entry.name = j.at("name").get<std::string>();
    entry.size_bytes = j.at("size_bytes").get<uintmax_t>();
    entry.created_at = util::parsePostgresTimestamp(j.at("created_at").get<std::string>());
    entry.updated_at = util::parsePostgresTimestamp(j.at("updated_at").get<std::string>());
    entry.path = std::filesystem::path(j.at("path").get<std::string>());

    if (j.contains("parent_id") && !j["parent_id"].is_null()) entry.parent_id = j.at("parent_id").get<unsigned int>();
    else entry.parent_id.reset();
}

void vh::types::to_json(nlohmann::json& j, const std::vector<std::shared_ptr<FSEntry>>& entries) {
    j = nlohmann::json::array();
    for (const auto& entry : entries) {
        if (entry->isDirectory()) j.push_back(*std::static_pointer_cast<Directory>(entry));
        else j.push_back(*std::static_pointer_cast<File>(entry));
    }
}

std::vector<std::shared_ptr<FSEntry>> vh::types::merge_entries(
    const std::vector<std::shared_ptr<File>>& files,
    const std::vector<std::shared_ptr<Directory>>& directories) {
    std::vector<std::shared_ptr<FSEntry>> entries;
    entries.reserve(files.size() + directories.size());

    entries.insert(entries.end(), files.begin(), files.end());
    entries.insert(entries.end(), directories.begin(), directories.end());

    return entries;
}

std::vector<std::shared_ptr<FSEntry>> vh::types::fromS3XML(const std::u8string& xml) {
    std::unordered_map<std::u8string, std::shared_ptr<Directory>> directories;
    std::vector<std::shared_ptr<File>> files;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(reinterpret_cast<const char*>(xml.c_str()));

    if (!result) {
        std::cerr << "[fromS3XML] Failed to parse XML: " << result.description() << std::endl;
        return {};
    }

    pugi::xml_node root = doc.child("ListBucketResult");
    if (!root) {
        std::cerr << "[fromS3XML] Missing <ListBucketResult> root node" << std::endl;
        return {};
    }

    for (pugi::xml_node content : root.children("Contents")) {
        auto keyNode = content.child("Key");
        auto sizeNode = content.child("Size");
        auto modifiedNode = content.child("LastModified");

        if (!keyNode || !sizeNode || !modifiedNode) {
            std::cerr << "[fromS3XML] Skipping entry due to missing child elements" << std::endl;
            continue;
        }

        const std::u8string key = reinterpret_cast<const char8_t*>(keyNode.text().get());
        const std::string lastMod = modifiedNode.text().get();
        const uint64_t size = std::stoull(sizeNode.text().get());

        std::tm tm{};
        std::istringstream ss(lastMod);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        std::time_t ts = ss.fail() ? std::time(nullptr) : timegm(&tm);

        std::filesystem::path full_path = std::filesystem::path(reinterpret_cast<const char*>(key.c_str()));
        std::filesystem::path current;

        for (auto pit = full_path.begin(); pit != std::prev(full_path.end()); ++pit) {
            current /= *pit;

            const auto dir_str = current.u8string();

            if (!directories.contains(dir_str)) {
                auto dir = std::make_shared<Directory>();
                dir->path = current;
                dir->name = current.filename().string();
                dir->created_at = dir->updated_at = ts;
                directories[dir_str] = dir;
            }

            directories[dir_str]->updated_at = std::max(directories[dir_str]->updated_at, ts);
        }

        auto file = std::make_shared<File>(std::string(reinterpret_cast<const char*>(key.c_str())), size, ts);
        files.push_back(file);
    }

    std::vector<std::shared_ptr<FSEntry>> ordered;
    std::vector<std::shared_ptr<Directory>> dirList;
    for (const auto& [_, dir] : directories) dirList.push_back(dir);

    std::ranges::sort(dirList, [](const auto& a, const auto& b) {
        return std::distance(a->path.begin(), a->path.end()) < std::distance(b->path.begin(), b->path.end());
    });

    ordered.insert(ordered.end(), dirList.begin(), dirList.end());
    ordered.insert(ordered.end(), files.begin(), files.end());

    return ordered;
}

std::unordered_map<std::u8string, std::shared_ptr<FSEntry>> vh::types::groupEntriesByPath(const std::vector<std::shared_ptr<FSEntry>>& entries) {
    std::unordered_map<std::u8string, std::shared_ptr<FSEntry>> grouped;

    for (const auto& entry : entries) {
        if (grouped.contains(entry->path.u8string())) {
            std::cerr << "[groupEntriesByPath] Duplicate entry found for path: " << entry->path.string() << std::endl;
            continue;
        }
        grouped[entry->path.u8string()] = entry;
    }

    return grouped;
}

#include "types/FSEntry.hpp"
#include "shared_util/timestamp.hpp"
#include "types/Directory.hpp"
#include "types/File.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <regex>

using namespace vh::types;

FSEntry::FSEntry(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      vault_id(row["vault_id"].as<unsigned int>()),
      created_by(row["created_by"].as<unsigned int>()),
      last_modified_by(row["last_modified_by"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
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

std::vector<std::shared_ptr<FSEntry>> vh::types::fromS3XML(const std::string& xml) {
    std::unordered_map<std::string, std::shared_ptr<Directory>> directories;
    std::vector<std::shared_ptr<File>> files;

    const std::regex entryRe(
        R"(<Contents>[\s\S]*?<Key>([^<]+)</Key>[\s\S]*?<LastModified>([^<]+)</LastModified>[\s\S]*?<Size>(\d+)</Size>[\s\S]*?</Contents>)"
    );

    for (auto it = std::sregex_iterator(xml.begin(), xml.end(), entryRe); it != std::sregex_iterator(); ++it) {
        const std::string key = (*it)[1].str();
        const std::string lastMod = (*it)[2].str();
        const uint64_t size = std::stoull((*it)[3].str());

        // Parse timestamp
        std::tm tm{};
        std::istringstream ss(lastMod);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
        std::time_t ts = timegm(&tm);

        // Build parent directories
        std::filesystem::path full_path(key);
        std::filesystem::path current;

        for (auto pit = full_path.begin(); pit != std::prev(full_path.end()); ++pit) {
            current /= *pit;
            std::string dir_str = current.string();

            if (!directories.contains(dir_str)) {
                auto dir = std::make_shared<Directory>();
                dir->path = dir_str;
                dir->name = current.filename().string();
                dir->updated_at = dir->created_at = ts;
                directories[dir_str] = dir;
            }

            directories[dir_str]->updated_at = std::max(directories[dir_str]->updated_at, ts);
        }

        // Create file entry
        auto file = std::make_shared<File>(key, size, ts);
        files.push_back(file);
    }

    // Sort directories by path depth ascending
    std::vector<std::shared_ptr<FSEntry>> ordered;
    std::vector<std::shared_ptr<Directory>> dirList;
    for (const auto& [_, dir] : directories) {
        dirList.push_back(dir);
    }

    std::ranges::sort(dirList.begin(), dirList.end(), [](const std::shared_ptr<Directory>& a, const std::shared_ptr<Directory>& b) {
        const auto depth_a = std::distance(a->path.begin(), a->path.end());
        const auto depth_b = std::distance(b->path.begin(), b->path.end());
        return depth_a < depth_b;
    });

    // Add sorted directories first
    ordered.insert(ordered.end(), dirList.begin(), dirList.end());

    // Then append all file entries
    ordered.insert(ordered.end(), files.begin(), files.end());

    return ordered;
}

std::vector<std::shared_ptr<FSEntry>> vh::types::fromS3XML(const std::vector<std::string>& xmlVector) {
    std::vector<std::shared_ptr<FSEntry>> all;
    for (const auto& xml : xmlVector) {
        auto partial = fromS3XML(xml);
        all.insert(all.end(), partial.begin(), partial.end());
    }
    return all;
}

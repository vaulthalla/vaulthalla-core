#include "types/FSEntry.hpp"
#include "util/timestamp.hpp"
#include "types/Directory.hpp"
#include "types/File.hpp"
#include "logging/LogRegistry.hpp"
#include "database/Queries/DirectoryQueries.hpp"
#include "database/Queries/VaultQueries.hpp"
#include "util/fsPath.hpp"

#include <nlohmann/json.hpp>
#include <pqxx/row>
#include <unordered_map>
#include <unordered_set>
#include <sstream>
#include <pugixml.hpp>
#include <algorithm>
#include <ranges>

using namespace vh::types;
using namespace vh::util;
using namespace vh::logging;
using namespace vh::database;

FSEntry::FSEntry(const pqxx::row& row)
    : id(row["id"].as<unsigned int>()),
      name(row["name"].as<std::string>()),
      base32_alias(row["base32_alias"].as<std::string>()),
      size_bytes(row["size_bytes"].as<uintmax_t>()),
      path(std::filesystem::path(row["path"].as<std::string>())),
      created_at(parsePostgresTimestamp(row["created_at"].as<std::string>())),
      updated_at(parsePostgresTimestamp(row["updated_at"].as<std::string>())) {
    if (row["parent_id"].is_null()) parent_id = std::nullopt;
    else parent_id = row["parent_id"].as<unsigned int>();

    if (row["owner_uid"].is_null()) owner_uid = std::nullopt;
    else owner_uid = row["owner_uid"].as<unsigned int>();

    if (row["group_gid"].is_null()) group_gid = std::nullopt;
    else group_gid = row["group_gid"].as<unsigned int>();

    if (row["vault_id"].is_null()) vault_id = std::nullopt;
    else vault_id = row["vault_id"].as<unsigned int>();

    if (row["created_by"].is_null()) created_by = std::nullopt;
    else created_by = row["created_by"].as<unsigned int>();

    if (row["last_modified_by"].is_null()) last_modified_by = std::nullopt;
    else last_modified_by = row["last_modified_by"].as<unsigned int>();

    if (row["inode"].is_null()) inode = std::nullopt;
    else inode = row["inode"].as<ino_t>();

    if (row["mode"].is_null()) mode = std::nullopt;
    else mode = row["mode"].as<mode_t>();

    if (row["is_hidden"].is_null()) is_hidden = false;
    else is_hidden = row["is_hidden"].as<bool>();

    if (row["is_system"].is_null()) is_system = false;
    else is_system = row["is_system"].as<bool>();

    if (parent_id) {
        auto parents = DirectoryQueries::collectParents(*parent_id);
        fuse_path = makeAbsolute(parents.back()->name) / stripLeadingSlash(path);

        backing_path = fs::path("/");
        for (const auto& parent : parents | std::views::reverse)
            backing_path /= parent->base32_alias;
    }
}

FSEntry::FSEntry(const std::string& s3_key) {
    name = std::filesystem::path(s3_key).filename().string();
    path = s3_key;
    created_at = updated_at = std::time(nullptr); // default timestamp (can override)
}

void vh::types::to_json(nlohmann::json& j, const FSEntry& entry) {
    j = {
        {"id", entry.id},
        {"is_hidden", entry.is_hidden},
        {"is_system", entry.is_system},
        {"name", entry.name},
        {"size_bytes", entry.size_bytes},
        {"created_at", timestampToString(entry.created_at)},
        {"updated_at", timestampToString(entry.updated_at)},
        {"path", entry.path.string()},
        {"abs_path", entry.fuse_path.string()},
    };

    if (entry.parent_id) j["parent_id"] = entry.parent_id;
    if (entry.vault_id) j["vault_id"] = entry.vault_id;
    if (entry.created_by) j["created_by"] = entry.created_by;
    if (entry.last_modified_by) j["last_modified_by"] = entry.last_modified_by;
    if (entry.owner_uid) j["owner_uid"] = entry.owner_uid;
    if (entry.group_gid) j["group_gid"] = entry.group_gid;
    if (entry.inode) j["inode"] = entry.inode;
    if (entry.mode) j["mode"] = entry.mode;
}

void vh::types::from_json(const nlohmann::json& j, FSEntry& entry) {
    entry.id = j.at("id").get<unsigned int>();
    entry.vault_id = j.at("vault_id").get<unsigned int>();
    entry.created_by = j.at("created_by").get<unsigned int>();
    entry.last_modified_by = j.at("last_modified_by").get<unsigned int>();
    entry.name = j.at("name").get<std::string>();
    entry.size_bytes = j.at("size_bytes").get<uintmax_t>();
    entry.created_at = parsePostgresTimestamp(j.at("created_at").get<std::string>());
    entry.updated_at = parsePostgresTimestamp(j.at("updated_at").get<std::string>());
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
        LogRegistry::types()->error("[FSEntry] [fromS3XML] Failed to parse XML: {}", result.description());
        return {};
    }

    pugi::xml_node root = doc.child("ListBucketResult");
    if (!root) {
        LogRegistry::types()->error("[FSEntry] [fromS3XML] Missing root element 'ListBucketResult'");
        return {};
    }

    for (pugi::xml_node content : root.children("Contents")) {
        auto keyNode = content.child("Key");
        auto sizeNode = content.child("Size");
        auto modifiedNode = content.child("LastModified");

        if (!keyNode || !sizeNode || !modifiedNode) {
            LogRegistry::types()->warn("[FSEntry] [fromS3XML] Missing required child nodes in <Contents>");
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
            LogRegistry::types()->warn("[FSEntry] [groupEntriesByPath] Duplicate entry found for path: {}", entry->path.string());
            continue;
        }
        grouped[entry->path.u8string()] = entry;
    }

    return grouped;
}

void FSEntry::print() const {
    LogRegistry::types()->debug("[FSEntry]\n"
                                "  ID: {}\n"
                                "  Name: {}\n"
                                "  Size: {} bytes\n"
                                "  Path: {}\n"
                                "  Absolute Path: {}\n"
                                "  Hidden: {}\n"
                                "  System: {}\n"
                                "  Created At: {}\n"
                                "  Updated At: {}\n"
                                "  Parent ID: {}\n"
                                "  Vault ID: {}\n"
                                "  Created By: {}\n"
                                "  Last Modified By: {}\n"
                                "  Owner UID: {}\n"
                                "  Group GID: {}\n"
                                "  Inode: {}\n"
                                "  Mode: {}",
                                id,
                                name,
                                size_bytes,
                                path.string(),
                                fuse_path.string(),
                                is_hidden ? "Yes" : "No",
                                is_system ? "Yes" : "No",
                                timestampToString(created_at),
                                timestampToString(updated_at),
                                parent_id ? std::to_string(*parent_id) : "N/A",
                                vault_id ? std::to_string(*vault_id) : "N/A",
                                created_by ? std::to_string(*created_by) : "N/A",
                                last_modified_by ? std::to_string(*last_modified_by) : "N/A",
                                owner_uid ? std::to_string(*owner_uid) : "N/A",
                                group_gid ? std::to_string(*group_gid) : "N/A",
                                inode ? std::to_string(*inode) : "N/A",
                                mode ? std::to_string(*mode) : "N/A");
}


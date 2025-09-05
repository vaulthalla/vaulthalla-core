#include "database/Queries/FSEntryQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "util/u8.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::database;
using namespace vh::types;
using namespace vh::logging;

bool FSEntryQueries::rootExists() {
    return Transactions::exec("FSEntryQueries::rootExists", [&](pqxx::work& txn) {
        return txn.exec_prepared("root_entry_exists").one_field().as<bool>();
    });
}

std::shared_ptr<FSEntry> FSEntryQueries::getRootEntry() {
    return Transactions::exec("FSEntryQueries::getRootEntry", [&](pqxx::work& txn) -> std::shared_ptr<FSEntry> {
        const auto res = txn.exec_prepared("get_root_entry");
        if (res.empty()) {
            LogRegistry::db()->warn("[FSEntryQueries::getRootEntry] No root entry found in the database");
            return nullptr;
        }
        return std::make_shared<Directory>(res.one_row(), pqxx::result{});
    });
}

void FSEntryQueries::updateFSEntry(const std::shared_ptr<FSEntry>& entry) {
    Transactions::exec("FSEntryQueries::updateFSEntry", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(entry->inode);
        p.append(entry->vault_id);
        p.append(entry->parent_id);
        p.append(entry->name);
        p.append(entry->base32_alias);
        p.append(entry->last_modified_by);
        p.append(to_utf8_string(entry->path.u8string()));
        p.append(entry->mode);
        p.append(entry->owner_uid);
        p.append(entry->group_gid);
        p.append(entry->is_hidden);
        p.append(entry->is_system);

        txn.exec_prepared("update_fs_entry_by_inode", p);
    });
}

std::shared_ptr<FSEntry> FSEntryQueries::getFSEntry(const std::string& base32) {
    return Transactions::exec("FSEntryQueries::getFSEntry", [&](pqxx::work& txn) -> std::shared_ptr<FSEntry> {
        const auto fileRes = txn.exec_prepared("get_file_by_base32_alias", base32);
        if (!fileRes.empty()) {
            const auto parentRows = txn.exec_prepared("collect_parent_chain", fileRes.one_row()["parent_id"].as<std::optional<unsigned int>>());
            return std::make_shared<File>(fileRes.one_row(), parentRows);
        }

        const auto dirRes = txn.exec_prepared("get_dir_by_base32_alias", base32);
        if (!dirRes.empty()) {
            const auto parentRows = txn.exec_prepared("collect_parent_chain", dirRes.one_row()["parent_id"].as<std::optional<unsigned int>>());
            return std::make_shared<Directory>(dirRes.one_row(), parentRows);
        }

        return nullptr;
    });
}

std::shared_ptr<FSEntry> FSEntryQueries::getFSEntryByInode(const ino_t ino) {
    return Transactions::exec("FSEntryQueries::getFSEntryByInode", [&](pqxx::work& txn) -> std::shared_ptr<FSEntry> {
        const auto fileRes = txn.exec_prepared("get_file_by_inode", ino);
        if (!fileRes.empty()) {
            const auto parentRows = txn.exec_prepared("collect_parent_chain", fileRes.one_row()["parent_id"].as<std::optional<unsigned int>>());
            return std::make_shared<File>(fileRes.one_row(), parentRows);
        }

        const auto dirRes = txn.exec_prepared("get_dir_by_inode", ino);
        if (!dirRes.empty()) {
            const auto parentRows = txn.exec_prepared("collect_parent_chain", dirRes.one_row()["parent_id"].as<std::optional<unsigned int>>());
            return std::make_shared<Directory>(dirRes.one_row(), parentRows);
        }

        return nullptr;
    });
}


std::shared_ptr<FSEntry> FSEntryQueries::getFSEntryById(unsigned int entryId) {
    return Transactions::exec("FSEntryQueries::getFSEntryById", [&](pqxx::work& txn) -> std::shared_ptr<FSEntry> {
        const auto res = txn.exec_prepared("get_fs_entry_by_id", entryId);
        if (res.empty()) return nullptr;

        const auto fileRes = txn.exec_prepared("get_file_by_id", entryId);
        if (!fileRes.empty()) {
            const auto parentRows = txn.exec_prepared("collect_parent_chain", fileRes.one_row()["parent_id"].as<std::optional<unsigned int>>());
            return std::make_shared<File>(fileRes.one_row(), parentRows);
        }

        const auto dirRes = txn.exec_prepared("get_dir_by_id", entryId);
        if (!dirRes.empty()) {
            const auto parentRows = txn.exec_prepared("collect_parent_chain", dirRes.one_row()["parent_id"].as<std::optional<unsigned int>>());
            return std::make_shared<Directory>(dirRes.one_row(), parentRows);
        }

        return nullptr;
    });
}

void FSEntryQueries::renameEntry(const std::shared_ptr<FSEntry>& entry) {
    if (entry->parent_id && *entry->parent_id == 0) entry->parent_id = std::nullopt;
    Transactions::exec("FSEntryQueries::renameEntry", [&](pqxx::work& txn) {
        pqxx::params p{entry->id, entry->name,
                       to_utf8_string(entry->path.u8string()),
                       to_utf8_string(entry->fuse_path.u8string()),
                       entry->parent_id};
        txn.exec_prepared("rename_fs_entry", p);
    });
}

std::vector<std::shared_ptr<FSEntry> > FSEntryQueries::listDir(const std::optional<unsigned int>& entryId, const bool recursive) {
    if (!entryId) {
        LogRegistry::db()->warn("[FSEntryQueries::listDir] entryId is null, returning empty list");
        return {};
    }

    LogRegistry::db()->debug("[FSEntryQueries::listDir] Listing directory with parent ID: {}, recursive: {}", *entryId, recursive);

    return Transactions::exec("FSEntryQueries::listDirById", [&](pqxx::work& txn) {
        const auto files = files_from_pq_res(
            recursive
                ? txn.exec_prepared("list_files_in_dir_by_parent_id_recursive", entryId)
                : txn.exec_prepared("list_files_in_dir_by_parent_id", entryId)
            );

        LogRegistry::db()->info("[FSEntryQueries::listDir] Executing queries for directories...");
        const auto directories = directories_from_pq_res(
            recursive
                ? txn.exec_prepared("list_dirs_in_dir_by_parent_id_recursive", entryId)
                : txn.exec_prepared("list_dirs_in_dir_by_parent_id", entryId)
            );

        return merge_entries(files, directories);
    });
}

ino_t FSEntryQueries::getNextInode() {
    return Transactions::exec("FSEntryQueries::getNextInode", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("get_next_inode");
        if (res.empty()) throw std::runtime_error("No inode available");
        return res.one_field().as<ino_t>();
    });
}

pqxx::result FSEntryQueries::collectParentChain(unsigned int parentId) {
    return Transactions::exec("FSEntryQueries::collectParentChain", [&](pqxx::work& txn) {
        return txn.exec_prepared("collect_parent_chain", parentId);
    });
}


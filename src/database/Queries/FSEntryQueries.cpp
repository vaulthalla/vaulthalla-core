#include "database/Queries/FSEntryQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "util/u8.hpp"

using namespace vh::database;
using namespace vh::types;

void FSEntryQueries::upsertFSEntry(const std::shared_ptr<FSEntry>& entry) {
    if (entry->parent_id && *entry->parent_id == 0) entry->parent_id = std::nullopt;

    Transactions::exec("FSEntryQueries::upsertFSEntry", [&](pqxx::work& txn) {
        if (entry->inode) txn.exec_prepared("delete_fs_entry_by_inode", entry->inode);

        pqxx::params p{
            entry->vault_id,
            entry->parent_id,
            entry->name,
            to_utf8_string(entry->path.u8string()),
            to_utf8_string(entry->fuse_path.u8string()),
            entry->inode,
            entry->mode,
            entry->owner_uid,
            entry->group_gid,
            entry->is_hidden,
            entry->is_system
        };

        if (txn.exec_prepared("upsert_fs_entry", p).empty())
            throw std::runtime_error("Upsert failed: no row returned");
    });
}


std::shared_ptr<FSEntry> FSEntryQueries::getFSEntry(const fs::path& absPath) {
    return Transactions::exec("FSEntryQueries::getFSEntry", [&](pqxx::work& txn) -> std::shared_ptr<FSEntry> {
        const auto path_str = to_utf8_string(absPath.u8string());

        const auto fileRes = txn.exec_prepared("get_file_by_fuse_path", path_str);
        if (!fileRes.empty()) return std::make_shared<File>(fileRes[0]);

        const auto dirRes = txn.exec_prepared("get_dir_by_fuse_path", path_str);
        if (!dirRes.empty()) return std::make_shared<Directory>(dirRes[0]);

        return nullptr;
    });
}

std::shared_ptr<FSEntry> FSEntryQueries::getFSEntryById(unsigned int entryId) {
    return Transactions::exec("FSEntryQueries::getFSEntryById", [&](pqxx::work& txn) -> std::shared_ptr<FSEntry> {
        const auto res = txn.exec_prepared("get_fs_entry_by_id", entryId);
        if (res.empty()) return nullptr;

        const auto fileRes = txn.exec_prepared("get_file_by_id", entryId);
        if (!fileRes.empty()) return std::make_shared<File>(fileRes[0]);

        const auto dirRes = txn.exec_prepared("get_dir_by_id", entryId);
        if (!dirRes.empty()) return std::make_shared<Directory>(dirRes[0]);

        return nullptr;
    });
}

std::optional<unsigned int> FSEntryQueries::getEntryIdByPath(const fs::path& absPath) {
    return Transactions::exec("FSEntryQueries::getEntryIdByPath", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("get_fs_entry_id_by_fuse_path", to_utf8_string(absPath.u8string()));
        std::optional<unsigned int> entryId;
        if (res.empty()) entryId = std::nullopt;
        else entryId = res.one_field().as<unsigned int>();
        return entryId;
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

    const auto cached = getFSEntryById(entry->id);

    const auto didUpdate = [&]() {
        if (!cached) return false;
        if (cached->id != entry->id) return false;

        const bool changed =
            cached->name != entry->name ||
            cached->path != entry->path ||
            cached->fuse_path != entry->fuse_path ||
            cached->parent_id != entry->parent_id;

        if (!changed) return false;

        cached->name = entry->name;
        cached->path = entry->path;
        cached->fuse_path = entry->fuse_path;
        cached->parent_id = entry->parent_id;
        return true;
    };

    if (!didUpdate())
        throw std::runtime_error(
            "FSEntryQueries::renameEntry: Failed to update entry in cache after rename operation");
}

std::vector<std::shared_ptr<FSEntry> > FSEntryQueries::listDir(const fs::path& absPath, bool recursive) {
    return Transactions::exec("FSEntryQueries::listDir", [&](pqxx::work& txn) {
        const auto patterns = computePatterns(to_utf8_string(absPath.u8string()), recursive);
        pqxx::params p{patterns.like, patterns.not_like};

        const auto files = files_from_pq_res(
            recursive
                ? txn.exec_prepared("list_files_in_dir_by_fuse_path_recursive", pqxx::params{patterns.like})
                : txn.exec_prepared("list_files_in_dir_by_fuse_path", p)
            );

        const auto directories = directories_from_pq_res(
            recursive
                ? txn.exec_prepared("list_directories_in_dir_by_fuse_path_recursive", pqxx::params{patterns.like})
                : txn.exec_prepared("list_directories_in_dir_by_fuse_path", p)
            );

        return merge_entries(files, directories);
    });
}

std::vector<std::shared_ptr<FSEntry> > FSEntryQueries::listDir(unsigned int entryId, bool recursive) {
    return Transactions::exec("FSEntryQueries::listDirById", [&](pqxx::work& txn) {
        const auto files = files_from_pq_res(
            recursive
                ? txn.exec_prepared("list_files_in_dir_recursive_by_parent_id", entryId)
                : txn.exec_prepared("list_files_in_dir_by_parent_id", entryId)
            );

        const auto directories = directories_from_pq_res(
            recursive
                ? txn.exec_prepared("list_dirs_in_dir_recursive_by_parent_id", entryId)
                : txn.exec_prepared("list_dirs_in_dir_by_parent_id", entryId)
            );

        return merge_entries(files, directories);
    });
}

bool FSEntryQueries::exists(const fs::path& absPath) {
    return Transactions::exec("FSEntryQueries::exists", [&](pqxx::work& txn) {
        const auto path = to_utf8_string(absPath.u8string());
        return txn.exec("SELECT EXISTS(SELECT 1 FROM fs_entry WHERE fuse_path = "
                        + txn.quote(path) + ")").one_field().as<bool>();
    });
}

ino_t FSEntryQueries::getNextInode() {
    return Transactions::exec("FSEntryQueries::getNextInode", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("get_next_inode");
        if (res.empty()) throw std::runtime_error("No inode available");
        return res.one_field().as<ino_t>();
    });
}

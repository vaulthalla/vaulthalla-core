#include "util/fsDBHelpers.hpp"
#include "util/u8.hpp"
#include "types/File.hpp"
#include "types/FSEntry.hpp"

using namespace vh::database;

void vh::database::updateFile(pqxx::work& txn, const std::shared_ptr<types::File>& file) {
    const auto exists = txn.exec_prepared("fs_entry_exists_by_inode", file->inode).one_field().as<bool>();
    const auto sizeRes = txn.exec_prepared("get_file_size_by_inode", file->inode);
    const auto existingSize = sizeRes.empty() ? 0 : sizeRes.one_field().as<unsigned int>();

    pqxx::params p;
    p.append(file->id);
    p.append(file->size_bytes);
    p.append(file->mime_type);
    p.append(file->content_hash);
    p.append(file->encryption_iv);

    txn.exec_prepared("update_file_only", p);

    std::optional<unsigned int> parentId = file->parent_id;
    while (parentId) {
        pqxx::params stats_params{parentId, file->size_bytes - existingSize, exists ? 0 : 1, 0}; // Increment size_bytes and file_count
        txn.exec_prepared("update_dir_stats", stats_params);
        const auto res = txn.exec_prepared("get_fs_entry_parent_id", parentId);
        if (res.empty()) break;
        parentId = res.one_field().as<std::optional<unsigned int>>();
    }
}

void vh::database::updateFSEntry(pqxx::work& txn, const std::shared_ptr<types::FSEntry>& entry) {
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
}


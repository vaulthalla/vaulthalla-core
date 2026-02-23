#include "util/fsDBHelpers.hpp"
#include "util/u8.hpp"
#include "fs/model/Entry.hpp"
#include "fs/model/File.hpp"

using namespace vh::database;
using namespace vh::fs::model;

void vh::database::updateFile(pqxx::work& txn, const std::shared_ptr<File>& file) {
    const auto exists = txn.exec(pqxx::prepped{"fs_entry_exists_by_inode"}, file->inode).one_field().as<bool>();
    const auto sizeRes = txn.exec(pqxx::prepped{"get_file_size_by_inode"}, file->inode);
    const auto existingSize = sizeRes.empty() ? 0 : sizeRes.one_field().as<unsigned int>();

    pqxx::params p;
    p.append(file->id);
    p.append(file->size_bytes);
    p.append(file->mime_type);
    p.append(file->content_hash);
    p.append(file->encryption_iv);

    txn.exec(pqxx::prepped{"update_file_only"}, p);

    std::optional<unsigned int> parentId = file->parent_id;
    while (parentId) {
        pqxx::params stats_params{parentId, file->size_bytes - existingSize, exists ? 0 : 1, 0}; // Increment size_bytes and file_count
        txn.exec(pqxx::prepped{"update_dir_stats"}, stats_params);
        const auto res = txn.exec(pqxx::prepped{"get_fs_entry_parent_id"}, parentId);
        if (res.empty()) break;
        parentId = res.one_field().as<std::optional<unsigned int>>();
    }
}

void vh::database::updateFSEntry(pqxx::work& txn, const std::shared_ptr<Entry>& entry) {
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

    txn.exec(pqxx::prepped{"update_fs_entry_by_inode"}, p);
}


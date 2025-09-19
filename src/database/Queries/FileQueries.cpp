#include "database/Queries/FileQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/TrashedFile.hpp"
#include "util/u8.hpp"
#include "util/fsPath.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "storage/FSCache.hpp"

#include <optional>

using namespace vh::database;
using namespace vh::services;
using namespace vh::types;

unsigned int FileQueries::upsertFile(const std::shared_ptr<File>& file) {
    if (!file) throw std::invalid_argument("File cannot be null");
    if (!file->path.string().starts_with("/")) file->setPath("/" + to_utf8_string(file->path.u8string()));

    return Transactions::exec("FileQueries::addFile", [&](pqxx::work& txn) {
        const auto exists = txn.exec(pqxx::prepped{"fs_entry_exists_by_inode"}, file->inode).one_field().as<bool>();
        const auto sizeRes = txn.exec(pqxx::prepped{"get_file_size_by_inode"}, file->inode);
        const auto existingSize = sizeRes.empty() ? 0 : sizeRes.one_field().as<uintmax_t>();
        if (file->inode) txn.exec(pqxx::prepped{"delete_fs_entry_by_inode"}, file->inode);

        pqxx::params p;
        p.append(file->vault_id);
        p.append(file->parent_id);
        p.append(file->name);
        p.append(file->base32_alias);
        p.append(file->created_by);
        p.append(file->last_modified_by);
        p.append(to_utf8_string(file->path.u8string()));
        p.append(file->inode);
        p.append(file->mode);
        p.append(file->owner_uid);
        p.append(file->group_gid);
        p.append(file->is_hidden);
        p.append(file->is_system);
        p.append(file->size_bytes);
        p.append(file->mime_type);
        p.append(file->content_hash);
        p.append(file->encryption_iv);

        const auto fileId = txn.exec(pqxx::prepped{"upsert_file_full"}, p).one_row()["fs_entry_id"].as<unsigned int>();

        std::optional<unsigned int> parentId = file->parent_id;
        while (parentId) {
            int64_t size_delta = static_cast<int64_t>(file->size_bytes) - static_cast<int64_t>(existingSize);
            pqxx::params stats_params{parentId, size_delta, exists ? 0 : 1, 0}; // Increment size_bytes and file_count
            txn.exec(pqxx::prepped{"update_dir_stats"}, stats_params);
            const auto res = txn.exec(pqxx::prepped{"get_fs_entry_parent_id"}, parentId);
            if (res.empty()) break;
            parentId = res.one_field().as<std::optional<unsigned int>>();
        }

        return fileId;
    });
}

void FileQueries::updateFile(const std::shared_ptr<File>& file) {
    if (!file) throw std::invalid_argument("File cannot be null");
    if (file->id == 0) throw std::invalid_argument("File ID cannot be zero");
    if (!file->path.string().starts_with("/")) file->setPath("/" + to_utf8_string(file->path.u8string()));

    Transactions::exec("FileQueries::updateFile", [&](pqxx::work& txn) {
        const auto exists = txn.exec(pqxx::prepped{"fs_entry_exists_by_inode"}, file->inode).one_field().as<bool>();
        const auto sizeRes = txn.exec(pqxx::prepped{"get_file_size_by_inode"}, file->inode);
        const auto existingSize = sizeRes.empty() ? 0 : sizeRes.one_field().as<uintmax_t>();

        pqxx::params p;
        p.append(file->id);
        p.append(file->size_bytes);
        p.append(file->mime_type);
        p.append(file->content_hash);
        p.append(file->encryption_iv);

        txn.exec(pqxx::prepped{"update_file_only"}, p);

        std::optional<unsigned int> parentId = file->parent_id;
        while (parentId) {
            int64_t size_delta = static_cast<int64_t>(file->size_bytes) - static_cast<int64_t>(existingSize);
            pqxx::params stats_params{parentId, size_delta, exists ? 0 : 1, 0}; // Increment size_bytes and file_count
            txn.exec(pqxx::prepped{"update_dir_stats"}, stats_params);
            const auto res = txn.exec(pqxx::prepped{"get_fs_entry_parent_id"}, parentId);
            if (res.empty()) break;
            parentId = res.one_field().as<std::optional<unsigned int>>();
        }
    });
}

void FileQueries::markTrashedFileDeleted(const unsigned int id) {
    Transactions::exec("FileQueries::deleteFile", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"mark_trashed_file_deleted_by_id"}, id);
    });
}

void FileQueries::deleteFile(const unsigned int userId, const std::shared_ptr<File>& file) {
    Transactions::exec("FileQueries::deleteFileByPath", [&](pqxx::work& txn) {
        const auto path = to_utf8_string(file->path.u8string());

        pqxx::params p{file->vault_id, path};
        const auto row = txn.exec(pqxx::prepped{"get_file_parent_id_and_size_by_path"}, p).one_row();
        const auto parentId = row["parent_id"].as<std::optional<unsigned int> >();
        const auto sizeBytes = row["size_bytes"].as<unsigned int>();

        txn.exec(pqxx::prepped{"mark_file_trashed"}, pqxx::params{file->vault_id, path, userId});

        updateParentStatsAndCleanEmptyDirs(txn, parentId, sizeBytes);

        txn.exec(pqxx::prepped{"mark_trashed_file_deleted_fuse_path"}, to_utf8_string(file->fuse_path.u8string()));
    });
}

void FileQueries::moveFile(const std::shared_ptr<File>& file, const std::filesystem::path& newPath, unsigned int userId) {
    const auto commonPath = common_path_prefix(file->path, newPath);
    Transactions::exec("FileQueries::moveFile", [&](pqxx::work& txn) {
        // Update the file's path and parent_id up to the common path
        std::optional<unsigned int> parentId = file->parent_id;
        std::filesystem::path path = file->path;
        while (parentId && path != commonPath) {
            pqxx::params stats_params{parentId, -static_cast<int>(file->size_bytes), -1, 0};
            txn.exec(pqxx::prepped{"update_dir_stats"}, stats_params);
            const auto row = txn.exec(pqxx::prepped{"get_fs_entry_parent_id_and_path"}, parentId).one_row();
            parentId = row["parent_id"].as<std::optional<unsigned int>>();
            path = row["path"].as<std::string>();
        }

        // update the file's path and parent_id
        file->path = newPath;
        pqxx::params search_params{userId, to_utf8_string(newPath.parent_path().u8string())};
        file->parent_id = txn.exec(pqxx::prepped{"get_fs_entry_id_by_path"}, search_params).one_field().as<unsigned int>();
        file->last_modified_by = userId;

        pqxx::params p;
        p.append(file->vault_id);
        p.append(file->parent_id);
        p.append(file->name);
        p.append(file->created_by);
        p.append(file->last_modified_by);
        p.append(to_utf8_string(file->path.u8string()));
        p.append(file->size_bytes);
        p.append(file->mime_type);
        p.append(file->content_hash);

        txn.exec(pqxx::prepped{"upsert_file_full"}, p);

        // Update parent directories stats
        parentId = file->parent_id;
        path = file->path;
        while (parentId && path != commonPath) {
            pqxx::params stats_params{parentId, file->size_bytes, 1, 0}; // Increment size_bytes and file_count
            txn.exec(pqxx::prepped{"update_dir_stats"}, stats_params);
            const auto nextRow = txn.exec(pqxx::prepped{"get_fs_entry_parent_id_and_path"}, parentId).one_row();
            parentId = nextRow["parent_id"].as<std::optional<unsigned int> >();
            path = nextRow["path"].as<std::string>();
        }
    });
}

std::shared_ptr<File> FileQueries::getFileByPath(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::getFileByPath", [&](pqxx::work& txn) -> std::shared_ptr<File> {
        const auto res = txn.exec(pqxx::prepped{"get_file_by_path"}, pqxx::params{vaultId, to_utf8_string(relPath.u8string())});
        if (res.empty()) return nullptr;
        const auto parentRows = txn.exec(pqxx::prepped{"collect_parent_chain"}, res.one_row()["parent_id"].as<std::optional<unsigned int>>());
        return std::make_shared<File>(res.one_row(), parentRows);
    });
}

std::shared_ptr<File> FileQueries::getFileById(unsigned int id) {
    return Transactions::exec("FileQueries::getFileById", [&](pqxx::work& txn) -> std::shared_ptr<File> {
        const auto res = txn.exec(pqxx::prepped{"get_file_by_id"}, id);
        if (res.empty()) return nullptr; // No file found with the given ID
        const auto parentRows = txn.exec(pqxx::prepped{"collect_parent_chain"}, res.one_row()["parent_id"].as<std::optional<unsigned int>>());
        return std::make_shared<File>(res.one_row(), parentRows);
    });
}


std::string FileQueries::getMimeType(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::getMimeType", [&](pqxx::work& txn) -> std::string {
        pqxx::params p{vaultId, to_utf8_string(relPath.u8string())};
        const auto res = txn.exec(pqxx::prepped{"get_file_mime_type"}, p);
        if (res.empty()) return "application/octet-stream"; // Default MIME type if not found
        return res.one_field().as<std::string>();
    });
}

bool FileQueries::isFile(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::isFile", [&](pqxx::work& txn) -> bool {
        pqxx::params p{vaultId, to_utf8_string(relPath.u8string())};
        return txn.exec(pqxx::prepped{"is_file"}, p).one_row()["exists"].as<bool>();
    });
}

std::vector<std::shared_ptr<File>> FileQueries::listFilesInDir(
    const unsigned int vaultId, const std::filesystem::path& path, const bool recursive) {
    return Transactions::exec("FileQueries::listFilesInDir", [&](pqxx::work& txn) {
        const auto patterns = computePatterns(path.string(), recursive);
        const auto res = recursive
                             ? txn.exec(pqxx::prepped{"list_files_in_dir_recursive"}, pqxx::params{vaultId, patterns.like})
                             : txn.exec(pqxx::prepped{"list_files_in_dir"},
                                                 pqxx::params{vaultId, patterns.like, patterns.not_like});

        return files_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<TrashedFile>> FileQueries::listTrashedFiles(unsigned int vaultId) {
    return Transactions::exec("FileQueries::listTrashedFiles", [&](pqxx::work& txn) {
        const auto res = txn.exec(pqxx::prepped{"list_trashed_files"}, vaultId);
        return trashed_files_from_pq_res(res);
    });
}

void FileQueries::markFileAsTrashed(const unsigned int userId, const unsigned int vaultId,
                                    const std::filesystem::path& relPath, const bool isFuseCall) {
    const auto file = getFileByPath(vaultId, relPath);
    if (!file) throw std::runtime_error("[markFileAsTrashed] File not found: " + to_utf8_string(relPath.u8string()));

    Transactions::exec("FileQueries::markFileAsTrashed", [&](pqxx::work& txn) {
        const auto res = txn.exec(pqxx::prepped{"get_file_parent_id_and_size_by_path"},
                                           pqxx::params{vaultId, to_utf8_string(relPath.u8string())});
        if (res.empty()) throw std::runtime_error("[markFileAsTrashed] File not found: " + to_utf8_string(relPath.u8string()));

        const auto row = res[0];
        const auto parentId = row["parent_id"].as<std::optional<unsigned int> >();
        const auto sizeBytes = row["size_bytes"].as<unsigned int>();

        txn.exec(pqxx::prepped{"mark_file_trashed"}, pqxx::params{vaultId, to_utf8_string(relPath.u8string()), userId, to_utf8_string(file->backing_path.u8string())});

        updateParentStatsAndCleanEmptyDirs(txn, parentId, sizeBytes, isFuseCall);
    });
}

void FileQueries::markFileAsTrashed(const unsigned int userId, const unsigned int fsId, const bool isFuseCall) {
    const auto file = getFileById(fsId);
    if (!file) throw std::runtime_error("[markFileAsTrashed] File not found with ID: " + std::to_string(fsId));

    Transactions::exec("FileQueries::markFileAsTrashed", [&](pqxx::work& txn) {
        const auto row = txn.exec(pqxx::prepped{"get_file_parent_id_and_size"}, fsId).one_row();
        const auto parentId = row["parent_id"].as<std::optional<unsigned int>>();
        const auto sizeBytes = row["size_bytes"].as<unsigned int>();

        pqxx::params p{fsId, userId, to_utf8_string(file->backing_path.u8string())};
        txn.exec(pqxx::prepped{"mark_file_trashed_by_id"}, p);

        updateParentStatsAndCleanEmptyDirs(txn, parentId, sizeBytes, isFuseCall);
    });
}

void FileQueries::updateParentStatsAndCleanEmptyDirs(pqxx::work& txn,
                                                     std::optional<unsigned int> parentId,
                                                     const unsigned int sizeBytes,
                                                     const bool isFuseCall) {
    const auto vaultId = txn.exec("SELECT vault_id FROM fs_entry WHERE id = $1", parentId).one_field().as<std::optional<unsigned int>>();
    const auto stopAt = vaultId ?
    txn.exec(pqxx::prepped{"get_vault_root_dir_id_by_vault_id"}, *vaultId).one_field().as<unsigned int>()
    : txn.exec(pqxx::prepped{"get_fs_entry_id_by_path"}, pqxx::params{vaultId, "/"}).one_field().as<unsigned int>();

    int subDirsDeleted = 0;
    bool deleteDirs = !isFuseCall;
    while (parentId) {
        pqxx::params stats_params{parentId, -static_cast<long long>(sizeBytes), -1, subDirsDeleted};
        const auto fsCount = txn.exec(pqxx::prepped{"update_dir_stats"}, stats_params).one_field().as<unsigned int>();
        const auto parentRes = txn.exec(pqxx::prepped{"get_fs_entry_parent_id"}, *parentId);
        if (*parentId == stopAt) deleteDirs = false;
        if (deleteDirs && fsCount == 0) {
            const auto inode = txn.exec(pqxx::prepped{"get_fs_entry_inode"}, *parentId).one_field().as<ino_t>();
            txn.exec(pqxx::prepped{"delete_fs_entry"}, *parentId);
            ServiceDepsRegistry::instance().fsCache->evictIno(inode);
            --subDirsDeleted;
        }
        if (parentRes.empty()) break;
        parentId = parentRes.one_field().as<std::optional<unsigned int>>();
    }
}

std::optional<std::pair<std::string, unsigned int>>  FileQueries::getEncryptionIVAndVersion(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::getEncryptionIV", [&](pqxx::work& txn) -> std::optional<std::pair<std::string, unsigned int>> {
        pqxx::params p{vaultId, to_utf8_string(relPath.u8string())};
        const auto res = txn.exec(pqxx::prepped{"get_file_encryption_iv_and_version"}, p);
        if (res.empty()) return std::nullopt;
        const auto row = res.one_row();
        return std::make_pair(row["encryption_iv"].as<std::string>(), row["encrypted_with_key_version"].as<unsigned int>());
    });
}

void FileQueries::setEncryptionIVAndVersion(const std::shared_ptr<File>& f) {
    Transactions::exec("FileQueries::setEncryptionIVAndVersion", [&](pqxx::work& txn) {
        pqxx::params p{f->vault_id, to_utf8_string(f->path.u8string()), f->encryption_iv, f->encrypted_with_key_version};
        txn.exec(pqxx::prepped{"set_file_encryption_iv_and_version"}, p);
    });
}

std::string FileQueries::getContentHash(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::getContentHash", [&](pqxx::work& txn) -> std::string {
        pqxx::params p{vaultId, to_utf8_string(relPath.u8string())};
        return txn.exec(pqxx::prepped{"get_file_content_hash"}, p).one_field().as<std::string>();
    });
}

std::vector<std::shared_ptr<File>> FileQueries::getFilesOlderThanKeyVersion(unsigned int vaultId, unsigned int keyVersion) {
    return Transactions::exec("FileQueries::getFilesOlderThanKeyVersion", [&](pqxx::work& txn) {
        const auto res = txn.exec(pqxx::prepped{"get_files_older_than_key_version"}, pqxx::params{vaultId, keyVersion});
        if (res.empty()) return std::vector<std::shared_ptr<File>>();
        return files_from_pq_res(res);
    });
}


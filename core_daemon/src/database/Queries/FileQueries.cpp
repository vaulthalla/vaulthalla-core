#include "database/Queries/FileQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "shared_util/u8.hpp"
#include "util/fsPath.hpp"

#include <optional>

using namespace vh::database;

unsigned int FileQueries::upsertFile(const std::shared_ptr<types::File>& file) {
    if (!file) throw std::invalid_argument("File cannot be null");
    if (!file->path.string().starts_with("/")) file->setPath("/" + to_utf8_string(file->path.u8string()));

    return Transactions::exec("FileQueries::addFile", [&](pqxx::work& txn) {
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

        const auto fileId = txn.exec_prepared("upsert_file_full", p).one_row()["fs_entry_id"].as<unsigned int>();

        std::optional<unsigned int> parentId = file->parent_id;
        while (parentId) {
            pqxx::params stats_params{parentId, file->size_bytes, 1, 0}; // Increment size_bytes and file_count
            txn.exec_prepared("update_dir_stats", stats_params);
            parentId = txn.exec_prepared("get_fs_entry_parent_id", parentId).one_field().as<std::optional<unsigned
                int> >();
        }

        return fileId;
    });
}

void FileQueries::deleteFile(const unsigned int userId, const unsigned int fileId) {
    Transactions::exec("FileQueries::deleteFile", [&](pqxx::work& txn) {
        const auto isTrashed = txn.exec_prepared("is_file_trashed", fileId).one_field().as<bool>();

        if (!isTrashed) {
            const auto row = txn.exec_prepared("get_file_parent_id_and_size", fileId).one_row();
            const auto parentId = row["parent_id"].as<std::optional<unsigned int> >();
            const auto sizeBytes = row["size_bytes"].as<unsigned int>();

            txn.exec_prepared("mark_file_trashed_by_id", pqxx::params{fileId, userId});

            updateParentStatsAndCleanEmptyDirs(txn, parentId, sizeBytes);
        }

        txn.exec_prepared("mark_trashed_file_deleted", fileId);
    });
}

void FileQueries::deleteFile(const unsigned int userId, unsigned int vaultId, const std::filesystem::path& relPath) {
    Transactions::exec("FileQueries::deleteFileByPath", [&](pqxx::work& txn) {
        const auto isTrashed = txn.exec_prepared("is_file_trashed_by_path", pqxx::params{vaultId, relPath.string()}).one_field().as<bool>();

        if (!isTrashed) {
            pqxx::params p{vaultId, to_utf8_string(relPath.u8string())};
            const auto row = txn.exec_prepared("get_file_parent_id_and_size_by_path", p).one_row();
            const auto parentId = row["parent_id"].as<std::optional<unsigned int> >();
            const auto sizeBytes = row["size_bytes"].as<unsigned int>();

            txn.exec_prepared("mark_file_trashed", pqxx::params{vaultId, to_utf8_string(relPath.u8string()), userId});

            updateParentStatsAndCleanEmptyDirs(txn, parentId, sizeBytes);
        }

        txn.exec_prepared("mark_trashed_file_deleted_by_path", pqxx::params{vaultId, to_utf8_string(relPath.u8string())});
    });
}

void FileQueries::moveFile(const std::shared_ptr<types::File>& file, const std::filesystem::path& newPath, unsigned int userId) {
    const auto commonPath = common_path_prefix(file->path, newPath);
    Transactions::exec("FileQueries::moveFile", [&](pqxx::work& txn) {
        // Update the file's path and parent_id up to the common path
        std::optional<unsigned int> parentId = file->parent_id;
        std::filesystem::path path = file->path;
        while (parentId && path != commonPath) {
            pqxx::params stats_params{parentId, -static_cast<int>(file->size_bytes), -1, 0};
            txn.exec_prepared("update_dir_stats", stats_params).one_field().as<unsigned int>();
            const auto row = txn.exec_prepared("get_fs_entry_parent_id_and_path", parentId).one_row();
            parentId = row["parent_id"].as<std::optional<unsigned int>>();
            path = row["path"].as<std::string>();
        }

        // update the file's path and parent_id
        file->path = newPath;
        pqxx::params search_params{userId, to_utf8_string(newPath.parent_path().u8string())};
        file->parent_id = txn.exec_prepared("get_fs_entry_id_by_path", search_params).one_field().as<unsigned int>();
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

        txn.exec_prepared("upsert_file_full", p).one_row()["fs_entry_id"].as<unsigned int>();

        // Update parent directories stats
        parentId = file->parent_id;
        path = file->path;
        while (parentId && path != commonPath) {
            pqxx::params stats_params{parentId, file->size_bytes, 1, 0}; // Increment size_bytes and file_count
            txn.exec_prepared("update_dir_stats", stats_params);
            const auto nextRow = txn.exec_prepared("get_fs_entry_parent_id_and_path", parentId).one_row();
            parentId = nextRow["parent_id"].as<std::optional<unsigned int> >();
            path = nextRow["path"].as<std::string>();
        }
    });
}

std::shared_ptr<vh::types::File> FileQueries::getFileByPath(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::getFileByPath", [&](pqxx::work& txn) -> std::shared_ptr<vh::types::File> {
        const auto row = txn.exec_prepared("get_file_by_path", pqxx::params{vaultId, relPath.string()}).one_row();
        return std::make_shared<types::File>(row);
    });
}

std::string FileQueries::getMimeType(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::getMimeType", [&](pqxx::work& txn) -> std::string {
        pqxx::params p{vaultId, relPath.string()};
        return txn.exec_prepared("get_file_mime_type", p).one_row()["mime_type"].as<std::string>();
    });
}

bool FileQueries::isFile(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::isFile", [&](pqxx::work& txn) -> bool {
        pqxx::params p{vaultId, relPath.string()};
        return txn.exec_prepared("is_file", p).one_row()["exists"].as<bool>();
    });
}

std::vector<std::shared_ptr<vh::types::File> > FileQueries::listFilesInDir(
    const unsigned int vaultId, const std::filesystem::path& path, const bool recursive) {
    return Transactions::exec("FileQueries::listFilesInDir", [&](pqxx::work& txn) {
        const auto patterns = computePatterns(path.string(), recursive);
        const auto res = recursive
                             ? txn.exec_prepared("list_files_in_dir_recursive", pqxx::params{vaultId, patterns.like})
                             : txn.exec_prepared("list_files_in_dir",
                                                 pqxx::params{vaultId, patterns.like, patterns.not_like});

        return types::files_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<vh::types::File> > FileQueries::listTrashedFiles(unsigned int vaultId) {
    return Transactions::exec("FileQueries::listTrashedFiles", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("list_trashed_files", vaultId);
        return types::files_from_pq_res(res);
    });
}

void FileQueries::markFileAsTrashed(const unsigned int userId, const unsigned int vaultId,
                                    const std::filesystem::path& relPath) {
    Transactions::exec("FileQueries::markFileAsTrashed", [&](pqxx::work& txn) {
        const auto res = txn.exec_prepared("get_file_parent_id_and_size_by_path",
                                           pqxx::params{vaultId, to_utf8_string(relPath.u8string())});
        if (res.empty()) throw std::runtime_error("[markFileAsTrashed] File not found: " + relPath.string());

        const auto row = res[0];
        const auto parentId = row["parent_id"].as<std::optional<unsigned int> >();
        const auto sizeBytes = row["size_bytes"].as<unsigned int>();

        txn.exec_prepared("mark_file_trashed", pqxx::params{vaultId, to_utf8_string(relPath.u8string()), userId});

        updateParentStatsAndCleanEmptyDirs(txn, parentId, sizeBytes);
    });
}

void FileQueries::markFileAsTrashed(const unsigned int userId, const unsigned int fsId) {
    Transactions::exec("FileQueries::markFileAsTrashed", [&](pqxx::work& txn) {
        const auto row = txn.exec_prepared("get_file_parent_id_and_size", fsId).one_row();
        const auto parentId = row["parent_id"].as<std::optional<unsigned int> >();
        const auto sizeBytes = row["size_bytes"].as<unsigned int>();

        txn.exec_prepared("mark_file_trashed_by_id", pqxx::params{fsId, userId});

        updateParentStatsAndCleanEmptyDirs(txn, parentId, sizeBytes);
    });
}

void FileQueries::updateParentStatsAndCleanEmptyDirs(pqxx::work& txn,
                                                     std::optional<unsigned int> parentId,
                                                     const unsigned int sizeBytes) {
    const auto vaultId = txn.exec("SELECT vault_id FROM fs_entry WHERE id = $1", parentId).one_field().as<unsigned int>();
    const auto rootId = txn.exec_prepared("get_fs_entry_id_by_path", pqxx::params{vaultId, "/"}).one_field().as<unsigned int>();

    int subDirsDeleted = 0; // TODO: track subdirs
    while (parentId) {
        pqxx::params stats_params{parentId, -static_cast<int>(sizeBytes), -1, 0};
        const auto fsCount = txn.exec_prepared("update_dir_stats", stats_params).one_field().as<unsigned int>();
        const auto nextParent = txn.exec_prepared("get_fs_entry_parent_id", *parentId).one_field().as<std::optional<unsigned int>>();
        if (fsCount == 0 && *parentId != rootId) {
            txn.exec_prepared("delete_fs_entry", *parentId);
            --subDirsDeleted;
        }
        parentId = nextParent;
    }
}
#include "database/Queries/FileQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "shared_util/u8.hpp"

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
        p.append(file->size_bytes);
        p.append(file->mime_type);
        p.append(file->content_hash);
        p.append(to_utf8_string(file->path.u8string()));

        const auto fileId = txn.exec_prepared("upsert_file", p).one_row()["id"].as<unsigned int>();

        std::optional<unsigned int> parentId = file->parent_id;
        while (parentId) {
            pqxx::params stats_params{parentId, file->size_bytes, 1, 0}; // Increment size_bytes and file_count
            txn.exec_prepared("update_dir_stats", stats_params);
            parentId = txn.exec_prepared("get_dir_parent_id", parentId).one_field().as<std::optional<unsigned int> >();
        }

        return fileId;
    });
}

void FileQueries::deleteFile(const unsigned int fileId) {
    Transactions::exec("FileQueries::deleteFile", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM files WHERE id = " + txn.quote(fileId));
    });
}

void FileQueries::deleteFile(unsigned int vaultId, const std::filesystem::path& relPath) {
    Transactions::exec("FileQueries::deleteFileByPath", [&](pqxx::work& txn) {
        txn.exec_prepared("delete_file_by_vault_and_path", pqxx::params{vaultId, relPath.string()});
    });
}

std::string FileQueries::getMimeType(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::getMimeType", [&](pqxx::work& txn) -> std::string {
        pqxx::params p{vaultId, relPath.string()};
        return txn.exec_prepared("get_file_mime_type", p).one_row()["mime_type"].as<std::string>();
    });
}

std::shared_ptr<vh::types::File> FileQueries::getFile(const unsigned int fileId) {
    return Transactions::exec("FileQueries::getFile", [&](pqxx::work& txn) -> std::shared_ptr<types::File> {
        const auto row = txn.exec("SELECT * FROM files WHERE id = " + txn.quote(fileId)).one_row();
        return std::make_shared<types::File>(row);
    });
}

std::shared_ptr<vh::types::File> FileQueries::getFileByPath(const std::filesystem::path& path) {
    return Transactions::exec("FileQueries::getFileByPath", [&](pqxx::work& txn) -> std::shared_ptr<types::File> {
        const auto row = txn.exec("SELECT * FROM files WHERE path = " + txn.quote(path.string())).one_row();
        return std::make_shared<types::File>(row);
    });
}

std::optional<unsigned int>
FileQueries::getFileIdByPath(const unsigned int vaultId, const std::filesystem::path& path) {
    return Transactions::exec("FileQueries::getFileIdByPath", [&](pqxx::work& txn) -> std::optional<unsigned int> {
        pqxx::params p{vaultId, path.string()};
        const auto res = txn.exec_prepared("get_file_id_by_path", p);
        if (res.empty()) return std::nullopt;
        return res.one_row()["id"].as<unsigned int>();
    });
}

bool FileQueries::isFile(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::isFile", [&](pqxx::work& txn) -> bool {
        pqxx::params p{vaultId, relPath.string()};
        return txn.exec_prepared("is_file", p).one_row()["exists"].as<bool>();
    });
}

bool FileQueries::fileExists(unsigned int vaultId, const std::filesystem::path& relPath) {
    return isFile(vaultId, relPath);
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

void FileQueries::markFileAsTrashed(const unsigned int userId, const unsigned int vaultId, const std::filesystem::path& relPath) {
    Transactions::exec("FileQueries::markFileAsTrashed", [&](pqxx::work& txn) {
        // const auto res = txn.exec_prepared("get_file_parent_id_and_size_by_path", pqxx::params{vaultId, to_utf8_string(relPath.u8string())});
        // if (res.empty()) throw std::runtime_error("[markFileAsTrashed] File not found: " + relPath.string());

        txn.exec_prepared("mark_file_trashed", pqxx::params{vaultId, to_utf8_string(relPath.u8string()), userId});

        // const auto row = res[0];
        // const auto parentId = row["parent_id"].as<std::optional<unsigned int>>();
        // const auto sizeBytes = row["size_bytes"].as<unsigned int>();

        // TODO: fix this
        // updateParentStatsAndCleanEmptyDirs(txn, parentId, sizeBytes);
    });
}

void FileQueries::updateParentStatsAndCleanEmptyDirs(pqxx::work& txn,
                                               std::optional<unsigned int> parentId,
                                               const unsigned int sizeBytes) {
    while (parentId) {
        std::cout << "[updateParentStats] Updating stats for parentId: " << *parentId
                  << ", sizeDelta: -" << sizeBytes << std::endl;

        pqxx::params stats_params{parentId, -static_cast<int>(sizeBytes), -1, 0};
        const auto result = txn.exec_prepared("update_dir_stats", stats_params).one_row();
        const auto fCount = result["file_count"].as<unsigned int>();

        if (fCount == 0) {
            std::cout << "[updateParentStats] Deleting empty directory with id: " << *parentId << std::endl;
            txn.exec_prepared("delete_directory", parentId);
        }

        const auto res = txn.exec_prepared("get_dir_parent_id", parentId);
        if (res.empty()) break;

        parentId = res[0]["parent_id"].as<std::optional<unsigned int>>();
    }

    txn.commit();
}

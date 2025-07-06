#include "database/Queries/FileQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"

using namespace vh::database;

void FileQueries::addFile(const std::shared_ptr<types::File>& file) {
    if (!file) throw std::invalid_argument("File cannot be null");

    Transactions::exec("FileQueries::addFile" ,[&](pqxx::work& txn) {
        pqxx::params p;
        p.append(file->vault_id);
        p.append(file->parent_id);
        p.append(file->name);
        p.append(file->created_by);
        p.append(file->last_modified_by);
        p.append(file->size_bytes);
        p.append(file->mime_type);
        p.append(file->content_hash);
        p.append(file->path.string());

        txn.exec_prepared("insert_file", p);
    });
}

void FileQueries::updateFile(const std::shared_ptr<types::File>& file) {
    if (!file) throw std::invalid_argument("File cannot be null");

    Transactions::exec("FileQueries::updateFile", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(file->id);
        p.append(file->vault_id);
        p.append(file->parent_id);
        p.append(file->name);
        p.append(file->last_modified_by);
        p.append(file->size_bytes);
        p.append(file->mime_type);
        p.append(file->content_hash);
        p.append(file->path.string());

        txn.exec_prepared("update_file", p);
    });
}

void FileQueries::deleteFile(const unsigned int fileId) {
    Transactions::exec("FileQueries::deleteFile", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM files WHERE id = " + txn.quote(fileId));
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

std::optional<unsigned int> FileQueries::getFileIdByPath(const unsigned int vaultId, const std::filesystem::path& path) {
    return Transactions::exec("FileQueries::getFileIdByPath", [&](pqxx::work& txn) -> std::optional<unsigned int> {
        pqxx::params p{vaultId, path.string()};
        const auto res = txn.exec_prepared("get_file_id_by_path", p);
        if (res.empty()) return std::nullopt;
        return res.one_row()["id"].as<unsigned int>();
    });
}

unsigned int FileQueries::addDirectory(const types::Directory& directory) {
    return Transactions::exec("FileQueries::addDirectory", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(directory.vault_id);
        p.append(directory.parent_id);
        p.append(directory.name);
        p.append(directory.created_by);
        p.append(directory.last_modified_by);
        p.append(directory.path.string());

        const auto id = txn.exec_prepared("insert_directory", p).one_row()["id"].as<unsigned int>();

        pqxx::params stats_params{id, 0, 0, 0}; // Initialize stats with zero values
        txn.exec_prepared("insert_dir_stats", stats_params);

        return id;
    });
}

void FileQueries::updateDirectory(const std::shared_ptr<types::Directory>& directory) {
    if (!directory) throw std::invalid_argument("Directory cannot be null");

    Transactions::exec("FileQueries::updateDirectory", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(directory->id);
        p.append(directory->vault_id);
        p.append(directory->parent_id);
        p.append(directory->name);
        p.append(directory->last_modified_by);
        p.append(directory->path.string());

        txn.exec_prepared("update_directory", p);

        pqxx::params stats_params;
        stats_params.append(directory->id);
        stats_params.append(directory->stats->size_bytes);
        stats_params.append(directory->stats->file_count);
        stats_params.append(directory->stats->subdirectory_count);

        txn.exec_prepared("update_dir_stats", stats_params);
    });
}

void FileQueries::updateDirectoryStats(const std::shared_ptr<types::Directory>& directory) {
    Transactions::exec("FileQueries::updateDirectoryStats", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(directory->id);
        p.append(directory->stats->size_bytes);
        p.append(directory->stats->file_count);
        p.append(directory->stats->subdirectory_count);

        txn.exec_prepared("update_dir_stats", p);
    });
}


void FileQueries::deleteDirectory(const unsigned int directoryId) {
    Transactions::exec("FileQueries::deleteDirectory", [&](pqxx::work& txn) {
        txn.exec("DELETE FROM directories WHERE id = " + txn.quote(directoryId));
        txn.exec("DELETE FROM dir_stats WHERE directory_id = " + txn.quote(directoryId));
    });
}

std::shared_ptr<vh::types::Directory> FileQueries::getDirectory(const unsigned int directoryId) {
    return Transactions::exec("FileQueries::getDirectory", [&](pqxx::work& txn) -> std::shared_ptr<types::Directory> {
        const auto row = txn.exec("SELECT * FROM directories WHERE id = " + txn.quote(directoryId)).one_row();
        return std::make_shared<types::Directory>(row);
    });
}

std::shared_ptr<vh::types::Directory> FileQueries::getDirectoryByPath(const std::filesystem::path& path) {
    return Transactions::exec("FileQueries::getDirectoryByPath", [&](pqxx::work& txn) -> std::shared_ptr<types::Directory> {
        const auto row = txn.exec("SELECT * FROM directories WHERE path = " + txn.quote(path.string())).one_row();
        return std::make_shared<types::Directory>(row);
    });
}

std::optional<unsigned int> FileQueries::getDirectoryIdByPath(const unsigned int vaultId, const std::filesystem::path& path) {
    return Transactions::exec("FileQueries::getDirectoryIdByPath", [&](pqxx::work& txn) -> std::optional<unsigned int> {
        pqxx::params p{vaultId, path.string()};
        const auto res = txn.exec_prepared("get_directory_id_by_path", p);
        if (res.empty()) return std::nullopt;
        return res.one_row()["id"].as<unsigned int>();
    });
}

unsigned int FileQueries::getRootDirectoryId(const unsigned int vaultId) {
    return Transactions::exec("FileQueries::getRootDirectoryId", [&](pqxx::work& txn) -> unsigned int {
        const auto row = txn.exec("SELECT id FROM directories WHERE vault_id = " + txn.quote(vaultId) +
                                  " AND parent_id IS NULL").one_row();
        return row["id"].as<unsigned int>();
    });
}

std::vector<std::shared_ptr<vh::types::FSEntry>> FileQueries::listDir(const unsigned int vaultId, const std::string& absPath, const bool recursive) {
    return Transactions::exec("FileQueries::listFilesInDir", [&](pqxx::work& txn) {
        const auto patterns = computePatterns(absPath, recursive);
        pqxx::params p{vaultId, patterns.like, patterns.not_like};

        const auto files = types::files_from_pq_res(
            recursive
            ? txn.exec_prepared("list_files_in_dir_recursive", pqxx::params{vaultId, patterns.like})
            : txn.exec_prepared("list_files_in_dir", p)
        );

        const auto directories = types::directories_from_pq_res(
            recursive
            ? txn.exec_prepared("list_directories_in_dir_recursive", pqxx::params{vaultId, patterns.like})
            : txn.exec_prepared("list_directories_in_dir", p)
        );

        return types::merge_entries(files, directories);
    });
}


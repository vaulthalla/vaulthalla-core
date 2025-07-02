#include "database/Queries/FileQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"

#include <pqxx/prepared_statement>

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
        p.append(file->path);

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
        p.append(file->path);

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
        const auto row = txn.exec("SELECT * FROM files WHERE full_path = " + txn.quote(path.string())).one_row();
        return std::make_shared<types::File>(row);
    });
}

unsigned int FileQueries::getFileIdByPath(const std::filesystem::path& path) {
    return Transactions::exec("FileQueries::getFileIdByPath", [&](pqxx::work& txn) -> unsigned int {
        const auto row = txn.exec("SELECT id FROM files WHERE full_path = " + txn.quote(path.string())).one_row();
        return row["id"].as<unsigned int>();
    });
}

void FileQueries::addDirectory(const std::shared_ptr<types::Directory>& directory) {
    if (!directory) throw std::invalid_argument("Directory cannot be null");

    Transactions::exec("FileQueries::addDirectory", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(directory->vault_id);
        p.append(directory->parent_id);
        p.append(directory->name);
        p.append(directory->created_by);
        p.append(directory->last_modified_by);
        p.append(directory->path);

        txn.exec_prepared("insert_directory", p);

        pqxx::params stats_params;
        stats_params.append(directory->id);
        stats_params.append(directory->stats->size_bytes);
        stats_params.append(directory->stats->file_count);
        stats_params.append(directory->stats->subdirectory_count);

        txn.exec_prepared("insert_dir_stats", stats_params);
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
        p.append(directory->path);

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
        const auto row = txn.exec("SELECT * FROM directories WHERE full_path = " + txn.quote(path.string())).one_row();
        return std::make_shared<types::Directory>(row);
    });
}

std::vector<std::shared_ptr<vh::types::FSEntry>> FileQueries::listDir(const unsigned int vaultId, const std::string& absPath, const bool recursive) {
    return Transactions::exec("FileQueries::listFilesInDir", [&](pqxx::work& txn) -> std::vector<std::shared_ptr<types::FSEntry>> {
        pqxx::params p;
        p.append(vaultId);
        p.append(absPath);

        const auto files = types::files_from_pq_res(txn.exec_prepared("list_files_in_dir", p));
        const auto directories = types::directories_from_pq_res(txn.exec_prepared("list_directories_in_dir", p));

        return types::merge_entries(files, directories);
    });
}

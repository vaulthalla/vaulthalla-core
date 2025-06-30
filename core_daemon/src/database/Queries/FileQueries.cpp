#include "database/Queries/FileQueries.hpp"
#include "database/Transactions.hpp"
#include "types/db/File.hpp"

#include <pqxx/prepared_statement>

using namespace vh::database;

void FileQueries::addFile(const std::shared_ptr<types::File>& file) {
    if (!file) throw std::invalid_argument("File cannot be null");

    Transactions::exec("FileQueries::addFile" ,[&](pqxx::work& txn) {
        pqxx::params p;
        p.append(file->storage_volume_id);
        p.append(file->parent_id);
        p.append(file->name);
        p.append(file->is_directory);
        p.append(file->mode);
        p.append(file->uid);
        p.append(file->gid);
        p.append(file->created_by);
        p.append(file->current_version_size_bytes);
        p.append(file->full_path);

        txn.exec_prepared("insert_file", p);
    });
}

void FileQueries::updateFile(const std::shared_ptr<types::File>& file) {
    if (!file) throw std::invalid_argument("File cannot be null");



    Transactions::exec("FileQueries::updateFile", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(file->id);
        p.append(file->storage_volume_id);
        p.append(file->parent_id);
        p.append(file->name);
        p.append(file->is_directory);
        p.append(file->mode);
        p.append(file->uid);
        p.append(file->gid);
        p.append(file->created_by);
        p.append(file->current_version_size_bytes);
        p.append(file->full_path);

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

std::vector<std::shared_ptr<vh::types::File>> FileQueries::listFilesInDir(const unsigned int volumeId, const std::string& absPath, const bool recursive) {
    return Transactions::exec("FileQueries::listFilesInDir", [&](pqxx::work& txn) -> std::vector<std::shared_ptr<types::File>> {
        std::vector<std::shared_ptr<types::File>> files;

        pqxx::params p;
        p.append(volumeId);
        p.append(absPath + "%"); // Use LIKE for partial matching

        const auto res = txn.exec_prepared("list_files_in_dir", p);

        for (const auto& row : res) files.push_back(std::make_shared<types::File>(row));

        if (recursive) {
            // TODO: Implement recursive listing if needed
        }

        return files;
    });
}

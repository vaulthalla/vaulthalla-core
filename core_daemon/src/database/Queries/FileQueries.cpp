#include "database/Queries/FileQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "shared_util/u8.hpp"

#include <optional>

using namespace vh::database;

void FileQueries::addFile(const std::shared_ptr<types::File>& file) {
    if (!file) throw std::invalid_argument("File cannot be null");
    if (!file->path.string().starts_with("/")) file->setPath("/" + to_utf8_string(file->path.u8string()));

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
        p.append(to_utf8_string(file->path.u8string()));

        txn.exec_prepared("insert_file", p);

        std::optional<unsigned int> parentId = file->parent_id;
        while (parentId) {
            pqxx::params stats_params{parentId, file->size_bytes, 1, 0}; // Increment size_bytes and file_count
            txn.exec_prepared("update_dir_stats", stats_params);
            parentId = txn.exec_prepared("get_dir_parent_id", parentId).one_field().as<std::optional<unsigned int>>();
        }
    });
}

void FileQueries::updateFile(const std::shared_ptr<types::File>& file) {
    if (!file) throw std::invalid_argument("File cannot be null");

    Transactions::exec("FileQueries::updateFile", [&](pqxx::work& txn) {
        const auto prevSize = txn.exec_prepared("get_file_size_bytes", file->id).one_field().as<unsigned int>();
        const auto diff = file->size_bytes - prevSize;

        pqxx::params p;
        p.append(file->id);
        p.append(file->vault_id);
        p.append(file->parent_id);
        p.append(file->name);
        p.append(file->last_modified_by);
        p.append(file->size_bytes);
        p.append(file->mime_type);
        p.append(file->content_hash);
        p.append(to_utf8_string(file->path.u8string()));

        txn.exec_prepared("update_file", p);

        std::optional<unsigned int> parentId = file->parent_id;
        while (parentId) {
            pqxx::params stats_params{parentId, diff, 0, 0}; // Update size_bytes with diff, file_count remains unchanged
            txn.exec_prepared("update_dir_stats", stats_params);
            parentId = txn.exec_prepared("get_dir_parent_id", parentId).one_field().as<std::optional<unsigned int>>();
        }
    });
}

void FileQueries::deleteFile(const unsigned int fileId) {
    Transactions::exec("FileQueries::deleteFile", [&](pqxx::work& txn) {
        const auto row = txn.exec_prepared("get_file_parent_id_and_size", fileId).one_row();
        const auto parentId = row["parent_id"].as<std::optional<unsigned int>>();
        const auto sizeBytes = row["size_bytes"].as<unsigned int>();

        txn.exec("DELETE FROM files WHERE id = " + txn.quote(fileId));

        std::optional<unsigned int> currentParentId = parentId;
        while (currentParentId) {
            pqxx::params stats_params{currentParentId, -static_cast<int>(sizeBytes), -1, 0}; // Decrement size_bytes and file_count
            txn.exec_prepared("update_dir_stats", stats_params);
            currentParentId = txn.exec_prepared("get_dir_parent_id", currentParentId).one_field().as<std::optional<unsigned int>>();
        }
    });
}

void FileQueries::deleteFile(unsigned int vaultId, const std::filesystem::path& relPath) {
    Transactions::exec("FileQueries::deleteFileByPath", [&](pqxx::work& txn) {
        pqxx::params p{vaultId, relPath.string()};
        if (txn.exec_prepared("delete_file_by_vault_and_path", p).affected_rows() == 0)
            throw std::runtime_error("No file found for vault ID: " + std::to_string(vaultId) + " and path: " + relPath.string());
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
        p.append(to_utf8_string(directory.path.u8string()));

        const auto id = txn.exec_prepared("insert_directory", p).one_row()["id"].as<unsigned int>();

        pqxx::params stats_params{id, 0, 0, 0}; // Initialize stats with zero values
        txn.exec_prepared("insert_dir_stats", stats_params);

        return id;
    });
}

void FileQueries::addDirectory(const std::shared_ptr<types::Directory>& directory) {
    if (!directory->path.string().starts_with("/")) directory->setPath("/" + to_utf8_string(directory->path.u8string()));

    Transactions::exec("FileQueries::addDirectory", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(directory->vault_id);
        p.append(directory->parent_id);
        p.append(directory->name);
        p.append(directory->created_by);
        p.append(directory->last_modified_by);
        p.append(to_utf8_string(directory->path.u8string()));

        const auto id = txn.exec_prepared("insert_directory", p).one_row()["id"].as<unsigned int>();

        pqxx::params stats_params{id, 0, 0, 0}; // Initialize stats with zero values
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
        p.append(to_utf8_string(directory->path.u8string()));

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

void FileQueries::deleteDirectory(unsigned int vaultId, const std::filesystem::path& relPath) {
    Transactions::exec("FileQueries::deleteDirectoryByPath", [&](pqxx::work& txn) {
        pqxx::params p{vaultId, relPath.string()};
        if (txn.exec_prepared("delete_directory_by_vault_and_path", p).affected_rows() == 0)
            throw std::runtime_error("No directory found for vault ID: " + std::to_string(vaultId) + " and path: " + relPath.string());
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
        pqxx::params p{vaultId, "/"};
        return txn.exec_prepared("get_directory_id_by_path", p).one_row()["id"].as<unsigned int>();
    });
}

bool FileQueries::isDirectory(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::isDirectory", [&](pqxx::work& txn) -> bool {
        pqxx::params p{vaultId, relPath.string()};
        return txn.exec_prepared("is_directory", p).one_row()["exists"].as<bool>();
    });
}

bool FileQueries::isFile(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("FileQueries::isFile", [&](pqxx::work& txn) -> bool {
        pqxx::params p{vaultId, relPath.string()};
        return txn.exec_prepared("is_file", p).one_row()["exists"].as<bool>();
    });
}

bool FileQueries::directoryExists(unsigned int vaultId, const std::filesystem::path& relPath) {
    return isDirectory(vaultId, relPath);
}

bool FileQueries::fileExists(unsigned int vaultId, const std::filesystem::path& relPath) {
    return isFile(vaultId, relPath);
}

std::vector<std::shared_ptr<vh::types::File>> FileQueries::listFilesInDir(const unsigned int vaultId, const std::filesystem::path& path, const bool recursive) {
    return Transactions::exec("FileQueries::listFilesInDir", [&](pqxx::work& txn) {
        const auto patterns = computePatterns(path.string(), recursive);
        const auto res = recursive
            ? txn.exec_prepared("list_files_in_dir_recursive", pqxx::params{vaultId, patterns.like})
            : txn.exec_prepared("list_files_in_dir", pqxx::params{vaultId, patterns.like, patterns.not_like});

        return types::files_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<vh::types::Directory>> FileQueries::listDirectoriesInDir(const unsigned int vaultId, const std::filesystem::path& path, const bool recursive) {
    return Transactions::exec("FileQueries::listDirectoriesInDir", [&](pqxx::work& txn) {
        const auto patterns = computePatterns(path.string(), recursive);
        const auto res = recursive
            ? txn.exec_prepared("list_directories_in_dir_recursive", pqxx::params{vaultId, patterns.like})
            : txn.exec_prepared("list_directories_in_dir", pqxx::params{vaultId, patterns.like, patterns.not_like});

        return types::directories_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<vh::types::FSEntry>> FileQueries::listDir(const unsigned int vaultId, const std::string& absPath, const bool recursive) {
    return Transactions::exec("FileQueries::listDir", [&](pqxx::work& txn) {
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

#include "database/Queries/DirectoryQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "shared_util/u8.hpp"

#include <optional>

using namespace vh::database;


void DirectoryQueries::upsertDirectory(const std::shared_ptr<types::Directory>& directory) {
    if (!directory->path.string().starts_with("/")) directory->setPath("/" + to_utf8_string(directory->path.u8string()));

    Transactions::exec("DirectoryQueries::addDirectory", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(directory->vault_id);
        p.append(directory->parent_id);
        p.append(directory->name);
        p.append(directory->created_by);
        p.append(directory->last_modified_by);
        p.append(to_utf8_string(directory->path.u8string()));
        p.append(directory->size_bytes);
        p.append(directory->file_count);
        p.append(directory->subdirectory_count);

        txn.exec_prepared("upsert_directory", p);
    });
}

void DirectoryQueries::deleteDirectory(const unsigned int directoryId) {
    Transactions::exec("DirectoryQueries::deleteDirectory", [&](pqxx::work& txn) {
        txn.exec_prepared("delete_fs_entry", directoryId);
    });
}

void DirectoryQueries::deleteDirectory(unsigned int vaultId, const std::filesystem::path& relPath) {
    Transactions::exec("DirectoryQueries::deleteDirectoryByPath", [&](pqxx::work& txn) {
        txn.exec_prepared("delete_fs_entry_by_path", pqxx::params{vaultId, relPath.string()});
    });
}

std::optional<unsigned int> DirectoryQueries::getDirectoryIdByPath(const unsigned int vaultId, const std::filesystem::path& path) {
    return Transactions::exec("DirectoryQueries::getDirectoryIdByPath", [&](pqxx::work& txn) -> std::optional<unsigned int> {
        pqxx::params p{vaultId, path.string()};
        const auto res = txn.exec_prepared("get_fs_entry_id_by_path", p);
        if (res.empty()) return std::nullopt;
        return res.one_row()["id"].as<unsigned int>();
    });
}

unsigned int DirectoryQueries::getRootDirectoryId(const unsigned int vaultId) {
    return Transactions::exec("DirectoryQueries::getRootDirectoryId", [&](pqxx::work& txn) -> unsigned int {
        pqxx::params p{vaultId, "/"};
        return txn.exec_prepared("get_fs_entry_id_by_path", p).one_row()["id"].as<unsigned int>();
    });
}

bool DirectoryQueries::isDirectory(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("DirectoryQueries::isDirectory", [&](pqxx::work& txn) -> bool {
        pqxx::params p{vaultId, relPath.string()};
        return txn.exec_prepared("is_directory", p).one_row()["exists"].as<bool>();
    });
}

bool DirectoryQueries::directoryExists(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return isDirectory(vaultId, relPath);
}

std::vector<std::shared_ptr<vh::types::Directory>> DirectoryQueries::listDirectoriesInDir(const unsigned int vaultId, const std::filesystem::path& path, const bool recursive) {
    return Transactions::exec("DirectoryQueries::listDirectoriesInDir", [&](pqxx::work& txn) {
        const auto patterns = computePatterns(path.string(), recursive);
        const auto res = recursive
            ? txn.exec_prepared("list_directories_in_dir_recursive", pqxx::params{vaultId, patterns.like})
            : txn.exec_prepared("list_directories_in_dir", pqxx::params{vaultId, patterns.like, patterns.not_like});

        return types::directories_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<vh::types::FSEntry>> DirectoryQueries::listDir(const unsigned int vaultId, const std::string& absPath, const bool recursive) {
    return Transactions::exec("DirectoryQueries::listDir", [&](pqxx::work& txn) {
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

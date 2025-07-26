#include "database/Queries/DirectoryQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "util/u8.hpp"
#include "util/fsPath.hpp"

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
        p.append(to_utf8_string(directory->abs_path.u8string()));
        p.append(directory->inode);
        p.append(directory->mode);
        p.append(directory->owner_uid);
        p.append(directory->group_gid);
        p.append(directory->is_hidden);
        p.append(directory->is_system);
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

void DirectoryQueries::moveDirectory(const std::shared_ptr<types::Directory>& directory, const std::filesystem::path& newPath, unsigned int userId) {
    if (!directory) throw std::invalid_argument("Directory cannot be null");
    if (!newPath.string().starts_with("/")) throw std::invalid_argument("New path must start with '/'");

    const auto commonPath = common_path_prefix(directory->path, newPath);

    Transactions::exec("DirectoryQueries::moveDirectory", [&](pqxx::work& txn) {
        // update parents of the directory up to the common path
        std::optional<unsigned int> parentId = directory->parent_id;
        std::filesystem::path path = directory->path;
        while (parentId && path != commonPath) {
            pqxx::params stats_params{parentId, -static_cast<int>(directory->size_bytes), -static_cast<int>(directory->file_count), 0};
            txn.exec_prepared("update_dir_stats", stats_params).one_field().as<unsigned int>();
            const auto row = txn.exec_prepared("get_fs_entry_parent_id_and_path", parentId).one_row();
            parentId = row["parent_id"].as<std::optional<unsigned int>>();
            path = row["path"].as<std::string>();
        }

        // Update the directory's path and parent_id
        directory->path = newPath;
        pqxx::params search_params{userId, to_utf8_string(newPath.parent_path().u8string())};
        directory->parent_id = txn.exec_prepared("get_fs_entry_id_by_path", search_params).one_field().as<unsigned int>();
        directory->last_modified_by = userId;

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

        // Update parent directories stats
        parentId = directory->parent_id;
        path = directory->path;
        while (parentId && path != commonPath) {
            pqxx::params stats_params{parentId, directory->size_bytes, directory->file_count, 0}; // Increment size_bytes and file_count
            txn.exec_prepared("update_dir_stats", stats_params);
            const auto nextRow = txn.exec_prepared("get_fs_entry_parent_id_and_path", parentId).one_row();
            parentId = nextRow["parent_id"].as<std::optional<unsigned int>>();
            path = nextRow["path"].as<std::string>();
        }
    });
}

std::shared_ptr<vh::types::Directory> DirectoryQueries::getDirectoryByPath(const unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("DirectoryQueries::getDirectoryByPath", [&](pqxx::work& txn) {
        const auto row = txn.exec_prepared("get_dir_by_path", pqxx::params{vaultId, relPath.string()}).one_row();
        return std::make_shared<types::Directory>(row);
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


// FUSE

std::shared_ptr<vh::types::Directory> DirectoryQueries::getDirectoryByInode(ino_t inode) {
    return Transactions::exec("DirectoryQueries::getDirectoryByInode", [&](pqxx::work& txn) {
        const auto row = txn.exec_prepared("get_dir_by_inode", inode).one_row();
        return std::make_shared<types::Directory>(row);
    });
}

std::shared_ptr<vh::types::Directory> DirectoryQueries::getDirectoryByAbsPath(const std::filesystem::path& absPath) {
    return Transactions::exec("DirectoryQueries::getDirectoryByAbsPath", [&](pqxx::work& txn) {
        const auto row = txn.exec_prepared("get_dir_by_abs_path", absPath.string()).one_row();
        return std::make_shared<types::Directory>(row);
    });
}

std::vector<std::shared_ptr<vh::types::Directory>> DirectoryQueries::listDirectoriesAbsPath(const std::filesystem::path& absPath, const bool recursive) {
    return Transactions::exec("DirectoryQueries::listDirectoriesAbsPath", [&](pqxx::work& txn) {
        const auto patterns = computePatterns(absPath.string(), recursive);
        const auto res = recursive
            ? txn.exec_prepared("list_directories_in_dir_by_abs_path_recursive", pqxx::params{patterns.like})
            : txn.exec_prepared("list_directories_in_dir_by_abs_path", pqxx::params{patterns.like, patterns.not_like});

        return types::directories_from_pq_res(res);
    });
}

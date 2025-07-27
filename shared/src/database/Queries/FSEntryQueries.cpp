#include "database/Queries/FSEntryQueries.hpp"
#include "database/Transactions.hpp"
#include "types/FSEntry.hpp"
#include "types/File.hpp"
#include "types/Directory.hpp"
#include "util/u8.hpp"

using namespace vh::database;
using namespace vh::types;

std::shared_ptr<FSEntry> FSEntryQueries::getFSEntry(const fs::path& absPath) {
    return Transactions::exec("FSEntryQueries::getFSEntry", [&](pqxx::work& txn) -> std::shared_ptr<FSEntry> {
        const auto path_str = to_utf8_string(absPath.u8string());

        const auto fileRes = txn.exec_prepared("get_file_by_abs_path", path_str);
        if (!fileRes.empty()) return std::make_shared<File>(fileRes[0]);

        const auto dirRes = txn.exec_prepared("get_dir_by_abs_path", path_str);
        if (!dirRes.empty()) return std::make_shared<Directory>(dirRes[0]);

        return nullptr;
    });
}

std::vector<std::shared_ptr<FSEntry>> FSEntryQueries::listDir(const fs::path& absPath, bool recursive) {
    return Transactions::exec("FSEntryQueries::listDir", [&](pqxx::work& txn) {
        const auto patterns = computePatterns(to_utf8_string(absPath.u8string()), recursive);
        pqxx::params p{patterns.like, patterns.not_like};

        const auto files = types::files_from_pq_res(
            recursive
            ? txn.exec_prepared("list_files_in_dir_by_abs_path_recursive", pqxx::params{patterns.like})
            : txn.exec_prepared("list_files_in_dir_by_abs_path", p)
        );

        const auto directories = types::directories_from_pq_res(
            recursive
            ? txn.exec_prepared("list_directories_in_dir_by_abs_path_recursive", pqxx::params{patterns.like})
            : txn.exec_prepared("list_directories_in_dir_by_abs_path", p)
        );

        return merge_entries(files, directories);
    });
}

bool FSEntryQueries::exists(const fs::path& absPath) {
    return Transactions::exec("FSEntryQueries::exists", [&](pqxx::work& txn) {
        const auto path = to_utf8_string(absPath.u8string());
        return txn.exec("SELECT EXISTS(SELECT 1 FROM fs_entry WHERE abs_path = "
            + txn.quote(path) + ")").one_field().as<bool>();
    });
}

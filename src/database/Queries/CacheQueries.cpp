#include "database/queries/CacheQueries.hpp"
#include "database/Transactions.hpp"
#include "fs/cache/Record.hpp"
#include "database/encoding/u8.hpp"

using namespace vh::database;
using namespace vh::fs::cache;

void CacheQueries::upsertCacheIndex(const std::shared_ptr<Record>& index) {
    if (!index) throw std::invalid_argument("CacheIndex cannot be null");
    Transactions::exec("CacheQueries::upsertCacheIndex", [&](pqxx::work& txn) {
        pqxx::params p;
        p.append(index->vault_id);
        p.append(index->file_id);
        p.append(encoding::to_utf8_string(index->path.u8string()));
        p.append(to_string(index->type));
        p.append(index->size);

        txn.exec(pqxx::prepped{"upsert_cache_index"}, p);
    });
}

void CacheQueries::deleteCacheIndex(unsigned int indexId) {
    Transactions::exec("CacheQueries::deleteCacheIndex", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"delete_cache_index"}, pqxx::params{indexId});
    });
}

void CacheQueries::deleteCacheIndex(unsigned int vaultId, const std::filesystem::path& relPath) {
    Transactions::exec("CacheQueries::deleteCacheIndexByPath", [&](pqxx::work& txn) {
        txn.exec(pqxx::prepped{"delete_cache_index_by_path"}, pqxx::params{vaultId, to_utf8_string(relPath.u8string())});
    });
}

std::shared_ptr<Record> CacheQueries::getCacheIndex(unsigned int indexId) {
    return Transactions::exec("CacheQueries::getCacheIndex", [&](pqxx::work& txn) -> std::shared_ptr<Record> {
        const auto row = txn.exec(pqxx::prepped{"get_cache_index"}, pqxx::params{indexId}).one_row();
        return std::make_shared<Record>(row);
    });
}

std::shared_ptr<Record> CacheQueries::getCacheIndexByPath(unsigned int vaultId, const std::filesystem::path& path) {
    return Transactions::exec("CacheQueries::getCacheIndexByPath", [&](pqxx::work& txn) -> std::shared_ptr<Record> {
        const auto row = txn.exec(pqxx::prepped{"get_cache_index_by_path"}, pqxx::params{vaultId, encoding::to_utf8_string(path.u8string())}).one_row();
        return std::make_shared<Record>(row);
    });
}

std::vector<std::shared_ptr<Record>> CacheQueries::listCacheIndices(unsigned int vaultId, const std::filesystem::path& relPath, const bool recursive) {
    return Transactions::exec("CacheQueries::listCacheindices", [&](pqxx::work& txn) -> std::vector<std::shared_ptr<Record>> {
        pqxx::result res;

        if (relPath.empty()) res = txn.exec(pqxx::prepped{"list_cache_indices"}, pqxx::params{vaultId});
        else {
            const auto patterns = computePatterns(relPath.string(), recursive);
            if (recursive) res = txn.exec(pqxx::prepped{"list_cache_indices_by_path_recursive"}, pqxx::params{vaultId, patterns.like});
            else res = txn.exec(pqxx::prepped{"list_cache_indices_by_path"}, pqxx::params{vaultId, patterns.like, patterns.not_like});
        }

        return cache_indices_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<Record>> CacheQueries::listCacheIndicesByFile(unsigned int fileId) {
    return Transactions::exec("CacheQueries::listCacheIndicesByFile", [&](pqxx::work& txn) -> std::vector<std::shared_ptr<Record>> {
        const auto res = txn.exec(pqxx::prepped{"list_cache_indices_by_file"}, pqxx::params{fileId});
        return cache_indices_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<Record>> CacheQueries::listCacheIndicesByType(const unsigned int vaultId, const Record::Type& type) {
    return Transactions::exec("CacheQueries::listCacheIndicesByType", [&](pqxx::work& txn) -> std::vector<std::shared_ptr<Record>> {
        const auto res = txn.exec(pqxx::prepped{"list_cache_indices_by_type"}, pqxx::params{vaultId, to_string(type)});
        return cache_indices_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<Record>> CacheQueries::nLargestCacheIndicesByType(const unsigned int n, const unsigned int vaultId, const Record::Type& type) {
    return Transactions::exec("CacheQueries::nLargestCacheIndicesByType", [&](pqxx::work& txn) -> std::vector<std::shared_ptr<Record>> {
        const auto res = txn.exec(pqxx::prepped{"n_largest_cache_indices_by_type"}, pqxx::params{vaultId, to_string(type), n});
        return cache_indices_from_pq_res(res);
    });
}

std::vector<std::shared_ptr<Record>> CacheQueries::nLargestCacheIndices(const unsigned int n, const unsigned int vaultId, const std::filesystem::path& relPath, const bool recursive) {
    return Transactions::exec("CacheQueries::nLargestCacheIndicesByPath", [&](pqxx::work& txn) -> std::vector<std::shared_ptr<Record>> {
        pqxx::result res;

        if (relPath.empty()) res = txn.exec(pqxx::prepped{"n_largest_cache_indices"}, pqxx::params{vaultId, n});
        else {
            const auto patterns = computePatterns(relPath.string(), recursive);
            if (recursive) res = txn.exec(pqxx::prepped{"n_largest_cache_indices_by_path_recursive"}, pqxx::params{vaultId, patterns.like, n});
            else res = txn.exec(pqxx::prepped{"n_largest_cache_indices_by_path"}, pqxx::params{vaultId, patterns.like, patterns.not_like, n});
        }

        return cache_indices_from_pq_res(res);
    });
}

bool CacheQueries::cacheIndexExists(unsigned int vaultId, const std::filesystem::path& relPath) {
    return Transactions::exec("CacheQueries::cacheIndexExists", [&](pqxx::work& txn) -> bool {
        return txn.exec(pqxx::prepped{"cache_index_exists"}, pqxx::params{vaultId, encoding::to_utf8_string(relPath.u8string())}).one_row()["exists"].as<bool>();
    });
}

unsigned int CacheQueries::countCacheIndices(unsigned int vaultId, const std::optional<Record::Type>& type) {
    return Transactions::exec("CacheQueries::countCacheIndices", [&](pqxx::work& txn) -> unsigned int {
        if (type) return txn.exec(pqxx::prepped{"count_cache_indices_by_type"}, pqxx::params{vaultId, to_string(*type)}).one_row()["count"].as<unsigned int>();
        return txn.exec(pqxx::prepped{"count_cache_indices"}, pqxx::params{vaultId}).one_row()["count"].as<unsigned int>();
    });
}


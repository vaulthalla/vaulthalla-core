#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <pqxx/pqxx>

namespace vh::fs::model {
struct Entry;
struct File;
struct Directory;
namespace file { struct Trashed; }

namespace stats {
struct Extension;
}

}

namespace vh::database {

class FileQueries {
public:
    FileQueries() = default;

    static unsigned int upsertFile(const std::shared_ptr<fs::model::File>& file);

    static void updateFile(const std::shared_ptr<fs::model::File>& file);

    static void markTrashedFileDeleted(unsigned int id);

    static void deleteFile(unsigned int userId, const std::shared_ptr<fs::model::File>& file);

    [[nodiscard]] static std::string getMimeType(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static bool isFile(unsigned int vaultId, const std::filesystem::path& relPath);

    static std::shared_ptr<fs::model::File> getFileById(unsigned int id);

    static std::shared_ptr<fs::model::File> getFileByPath(unsigned int vaultId, const std::filesystem::path& relPath);

    static void moveFile(const std::shared_ptr<fs::model::File>& file, const std::filesystem::path& newPath, unsigned int userId);

    static std::vector<std::shared_ptr<fs::model::File>> listFilesInDir(unsigned int vaultId, const std::filesystem::path& path = {"/"}, bool recursive = true);

    static std::vector<std::shared_ptr<fs::model::file::Trashed>> listTrashedFiles(unsigned int vaultId);

    static void markFileAsTrashed(unsigned int userId, unsigned int vaultId, const std::filesystem::path& relPath, bool isFuseCall = false);

    static void markFileAsTrashed(unsigned int userId, unsigned int fsId, bool isFuseCall = false);

    static void updateParentStatsAndCleanEmptyDirs(pqxx::work& txn,
                                               std::optional<unsigned int> parentId,
                                               unsigned int sizeBytes,
                                               bool isFuseCall = false);

    [[nodiscard]] static std::optional<std::pair<std::string, unsigned int>> getEncryptionIVAndVersion(unsigned int vaultId, const std::filesystem::path& relPath);

    static void setEncryptionIVAndVersion(const std::shared_ptr<fs::model::File>& f);

    static std::vector<std::shared_ptr<fs::model::File>> getFilesOlderThanKeyVersion(unsigned int vaultId, unsigned int keyVersion);

    [[nodiscard]] static std::string getContentHash(unsigned int vaultId, const std::filesystem::path& relPath);

    static std::shared_ptr<fs::model::File> getLargestFile(unsigned int vaultId);

    static std::vector<std::shared_ptr<fs::model::File>> getNLargestFiles(unsigned int vaultId, unsigned int n = 1);

    static std::vector<std::shared_ptr<fs::model::File>> getAllFiles(unsigned int vaultId);

    static std::vector<fs::model::stats::Extension> getTopExtensionsBySize(unsigned int vaultId, unsigned int limit = 10);
};

} // namespace vh::database
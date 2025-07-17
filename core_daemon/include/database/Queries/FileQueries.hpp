#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <pqxx/pqxx>

namespace vh::types {
struct FSEntry;
struct File;
struct Directory;
struct DirectoryStats;
}

namespace vh::database {

class FileQueries {
public:
    FileQueries() = default;

    static unsigned int upsertFile(const std::shared_ptr<types::File>& file);

    static void deleteFile(unsigned int fileId);

    static void deleteFile(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static std::string getMimeType(unsigned int vaultId, const std::filesystem::path& relPath);

    static std::shared_ptr<types::File> getFile(unsigned int fileId);

    static std::shared_ptr<types::File> getFileByPath(const std::filesystem::path& path);

    [[nodiscard]] static std::optional<unsigned int> getFileIdByPath(unsigned int vaultId, const std::filesystem::path& path);

    [[nodiscard]] static bool isFile(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static bool fileExists(unsigned int vaultId, const std::filesystem::path& relPath);

    static std::vector<std::shared_ptr<types::File>> listFilesInDir(unsigned int vaultId, const std::filesystem::path& path = {"/"}, bool recursive = true);

    static std::vector<std::shared_ptr<types::File>> listTrashedFiles(unsigned int vaultId);

    static void markFileAsTrashed(unsigned int userId, unsigned int vaultId, const std::filesystem::path& relPath);

    static void updateParentStatsAndCleanEmptyDirs(pqxx::work& txn,
                                               std::optional<unsigned int> parentId,
                                               unsigned int sizeBytes);
};

} // namespace vh::database
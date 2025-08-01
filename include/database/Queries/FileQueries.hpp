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
struct TrashedFile;
struct Directory;
struct DirectoryStats;
}

namespace vh::database {

class FileQueries {
public:
    FileQueries() = default;

    static unsigned int upsertFile(const std::shared_ptr<types::File>& file);

    static void markTrashedFileDeleted(unsigned int id);

    static void deleteFile(unsigned int userId, const std::shared_ptr<types::File>& file);

    [[nodiscard]] static std::string getMimeType(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static bool isFile(unsigned int vaultId, const std::filesystem::path& relPath);

    static std::shared_ptr<types::File> getFileByPath(unsigned int vaultId, const std::filesystem::path& relPath);

    static void moveFile(const std::shared_ptr<types::File>& file, const std::filesystem::path& newPath, unsigned int userId);

    static std::vector<std::shared_ptr<types::File>> listFilesInDir(unsigned int vaultId, const std::filesystem::path& path = {"/"}, bool recursive = true);

    static std::vector<std::shared_ptr<types::TrashedFile>> listTrashedFiles(unsigned int vaultId);

    static void markFileAsTrashed(unsigned int userId, unsigned int vaultId, const std::filesystem::path& relPath);

    static void markFileAsTrashed(unsigned int userId, unsigned int fsId);

    static void updateParentStatsAndCleanEmptyDirs(pqxx::work& txn,
                                               std::optional<unsigned int> parentId,
                                               unsigned int sizeBytes);

    [[nodiscard]] static std::string getEncryptionIV(unsigned int vaultId, const std::filesystem::path& relPath);

    static void setEncryptionIV(unsigned int vaultId, const std::filesystem::path& relPath, const std::string& iv);

    [[nodiscard]] static std::string getContentHash(unsigned int vaultId, const std::filesystem::path& relPath);

    // FUSE
    static std::shared_ptr<types::File> getFileByAbsPath(const std::filesystem::path& absPath);

    static std::shared_ptr<types::File> getFileByInode(ino_t inode);

    static std::vector<std::shared_ptr<types::File>> listFilesAbsPath(const std::filesystem::path& absPath, bool recursive = false);

};

} // namespace vh::database
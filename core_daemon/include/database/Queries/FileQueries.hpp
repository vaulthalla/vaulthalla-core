#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <optional>

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

    [[nodiscard]] static unsigned int addFile(const std::shared_ptr<types::File>& file);

    static void updateFile(const std::shared_ptr<types::File>& file);

    static void deleteFile(unsigned int fileId);

    [[nodiscard]] static std::string getMimeType(unsigned int vaultId, const std::filesystem::path& relPath);

    static std::shared_ptr<types::File> getFile(unsigned int fileId);

    static std::shared_ptr<types::File> getFileByPath(const std::filesystem::path& path);

    [[nodiscard]] static std::optional<unsigned int> getFileIdByPath(unsigned int vaultId, const std::filesystem::path& path);

    [[nodiscard]] static unsigned int addDirectory(const types::Directory& directory);

    static void addDirectory(const std::shared_ptr<types::Directory>& directory);

    static void updateDirectory(const std::shared_ptr<types::Directory>& directory);

    static void updateDirectoryStats(const std::shared_ptr<types::Directory>& directory);

    static void deleteDirectory(unsigned int directoryId);

    [[nodiscard]] static std::optional<unsigned int> getDirectoryIdByPath(unsigned int vaultId, const std::filesystem::path& path);

    [[nodiscard]] static unsigned int getRootDirectoryId(unsigned int vaultId);

    static std::shared_ptr<types::Directory> getDirectory(unsigned int directoryId);

    static std::shared_ptr<types::Directory> getDirectoryByPath(const std::filesystem::path& path);

    static std::vector<std::shared_ptr<types::FSEntry> > listDir(unsigned int vaultId, const std::string& absPath,
                                                                 bool recursive = false);
};

} // namespace vh::database
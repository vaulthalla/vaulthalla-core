#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

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

    static void addFile(const std::shared_ptr<types::File>& file);

    static void updateFile(const std::shared_ptr<types::File>& file);

    static void deleteFile(unsigned int fileId);

    static std::shared_ptr<types::File> getFile(unsigned int fileId);

    static std::shared_ptr<types::File> getFileByPath(const std::filesystem::path& path);

    [[nodiscard]] static unsigned int getFileIdByPath(const std::filesystem::path& path);

    static void addDirectory(const std::shared_ptr<types::Directory>& directory);

    static void updateDirectory(const std::shared_ptr<types::Directory>& directory);

    static void updateDirectoryStats(const std::shared_ptr<types::Directory>& directory);

    static void deleteDirectory(unsigned int directoryId);

    static std::shared_ptr<types::Directory> getDirectory(unsigned int directoryId);

    static std::shared_ptr<types::Directory> getDirectoryByPath(const std::filesystem::path& path);

    static std::vector<std::shared_ptr<types::FSEntry> > listDir(unsigned int vaultId, const std::string& absPath,
                                                                 bool recursive = false);
};

} // namespace vh::database
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

class DirectoryQueries {
public:
    DirectoryQueries() = default;

    [[nodiscard]] static unsigned int addDirectory(const types::Directory& directory);

    static void addDirectory(const std::shared_ptr<types::Directory>& directory);

    static void updateDirectory(const std::shared_ptr<types::Directory>& directory);

    static void updateDirectoryStats(const std::shared_ptr<types::Directory>& directory);

    static void deleteDirectory(unsigned int directoryId);

    static void deleteDirectory(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static bool isDirectory(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static bool directoryExists(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static std::optional<unsigned int> getDirectoryIdByPath(unsigned int vaultId, const std::filesystem::path& path);

    [[nodiscard]] static unsigned int getRootDirectoryId(unsigned int vaultId);

    static std::shared_ptr<types::Directory> getDirectory(unsigned int directoryId);

    static std::shared_ptr<types::Directory> getDirectoryByPath(unsigned int vaultId, const std::filesystem::path& path);

    static std::vector<std::shared_ptr<types::Directory>> listDirectoriesInDir(unsigned int vaultId, const std::filesystem::path& path, bool recursive = true);

    static std::vector<std::shared_ptr<types::FSEntry> > listDir(unsigned int vaultId, const std::string& absPath,
                                                                 bool recursive = false);
};

} // namespace vh::database
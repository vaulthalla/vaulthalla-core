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
}

namespace vh::database {

class DirectoryQueries {
public:
    DirectoryQueries() = default;

    static void upsertDirectory(const std::shared_ptr<types::Directory>& directory);

    static void deleteDirectory(unsigned int directoryId);

    static void deleteDirectory(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static bool isDirectory(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static bool directoryExists(unsigned int vaultId, const std::filesystem::path& relPath);

    static void moveDirectory(const std::shared_ptr<types::Directory>& directory, const std::filesystem::path& newPath, unsigned int userId);

    static std::shared_ptr<types::Directory> getDirectoryByPath(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static std::optional<unsigned int> getDirectoryIdByPath(unsigned int vaultId, const std::filesystem::path& path);

    [[nodiscard]] static unsigned int getRootDirectoryId(unsigned int vaultId);

    static std::vector<std::shared_ptr<types::Directory>> listDirectoriesInDir(unsigned int vaultId, const std::filesystem::path& path, bool recursive = true);

    static std::vector<std::shared_ptr<types::FSEntry> > listDir(unsigned int vaultId, const std::string& absPath,
                                                                 bool recursive = false);
};

} // namespace vh::database
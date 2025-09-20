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

namespace pqxx {
class result;
}

namespace vh::database {

class DirectoryQueries {
public:
    DirectoryQueries() = default;

    static unsigned int upsertDirectory(const std::shared_ptr<types::Directory>& directory);

    [[nodiscard]] static bool isDirectory(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static bool directoryExists(unsigned int vaultId, const std::filesystem::path& relPath);

    static void moveDirectory(const std::shared_ptr<types::Directory>& directory, const std::filesystem::path& newPath, unsigned int userId);

    static std::shared_ptr<types::Directory> getDirectoryByPath(unsigned int vaultId, const std::filesystem::path& relPath);

    [[nodiscard]] static std::optional<unsigned int> getDirectoryIdByPath(unsigned int vaultId, const std::filesystem::path& path);

    [[nodiscard]] static unsigned int getRootDirectoryId(unsigned int vaultId);

    static std::vector<std::shared_ptr<types::Directory>> listDirectoriesInDir(unsigned int parentId, bool recursive = false);

    [[nodiscard]] static pqxx::result collectParentStats(unsigned int parentId);

    static void deleteEmptyDirectory(unsigned int id);

    [[nodiscard]] static bool isDirectoryEmpty(unsigned int id);
};

} // namespace vh::database
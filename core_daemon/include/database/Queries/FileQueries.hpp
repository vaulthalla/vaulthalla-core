#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace vh::types {
struct File;
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

    static std::vector<std::shared_ptr<types::File> > listFilesInDir(unsigned int volumeId, const std::string& absPath,
                                                              bool recursive = false);
};

} // namespace vh::database
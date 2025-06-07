#pragma once

#include <string>
#include <memory>
#include "FileMetadata.hpp"

namespace vh::core {

    enum class FileType {
        File,
        Directory,
        Symlink,
        Unknown
    };

    class FileNode {
    public:
        FileNode(std::string  path, FileType type);

        [[nodiscard]] const std::string& get_path() const noexcept;
        [[nodiscard]] FileType get_type() const noexcept;
        [[nodiscard]] const FileMetadata& get_metadata() const;
        void set_metadata(const FileMetadata& metadata);

    private:
        std::string path_;
        FileType type_;
        std::shared_ptr<FileMetadata> metadata_;
    };

} // namespace vh::core

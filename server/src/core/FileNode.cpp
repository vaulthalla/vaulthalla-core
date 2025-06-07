#include "core/FileNode.hpp"

#include <utility>

namespace vh::core {

    FileNode::FileNode(std::string  path, FileType type)
            : path_(std::move(path)), type_(type), metadata_(std::make_shared<FileMetadata>()) {}

    const std::string& FileNode::get_path() const noexcept {
        return path_;
    }

    FileType FileNode::get_type() const noexcept {
        return type_;
    }

    const FileMetadata& FileNode::get_metadata() const {
        return *metadata_;
    }

    void FileNode::set_metadata(const FileMetadata& metadata) {
        *metadata_ = metadata;
    }

} // namespace vh::core

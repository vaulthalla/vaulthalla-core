#pragma once

#include "engine/StorageEngineBase.hpp"

#include <filesystem>
#include <utility>
#include <memory>

namespace fs = std::filesystem;

namespace vh::types {
struct Vault;
struct File;
}

namespace vh::services {
class ThumbnailWorker;
}

namespace vh::storage {

class StorageEngine : public engine::StorageEngineBase {
public:
    StorageEngine() = default;
    ~StorageEngine() override = default;

    explicit StorageEngine(const std::shared_ptr<types::Vault>& vault);

    void finishUpload(unsigned int userId, const fs::path& relPath);

    void mkdir(const fs::path& relPath, unsigned int userId) const;

    void move(const fs::path& from, const fs::path& to, unsigned int userId) const;

    void rename(const fs::path& from, const fs::path& to, unsigned int userId) const;

    void copy(const fs::path& from, const fs::path& to, unsigned int userId) const;

    void remove(const fs::path& rel_path, unsigned int userId) const;

protected:
    void removeFile(const fs::path& rel_path, unsigned int userId) const;
    void removeDirectory(const fs::path& rel_path, unsigned int userId) const;
};

} // namespace vh::storage
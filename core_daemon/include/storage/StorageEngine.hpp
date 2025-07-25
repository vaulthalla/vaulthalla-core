#pragma once

#include "engine/StorageEngineBase.hpp"

#include <filesystem>
#include <optional>
#include <utility>
#include <vector>
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

    explicit StorageEngine(const std::shared_ptr<types::Vault>& vault,
                           const std::shared_ptr<services::ThumbnailWorker>& thumbWorker);

    void finishUpload(unsigned int userId, const fs::path& relPath);

    void mkdir(const fs::path& relPath, unsigned int userId) const;

    void move(const fs::path& from, const fs::path& to, unsigned int userId) const;

    void rename(const fs::path& from, const fs::path& to, unsigned int userId) const;

    void copy(const fs::path& from, const fs::path& to, unsigned int userId) const;

    [[nodiscard]] virtual std::optional<std::vector<uint8_t> > readFile(const fs::path& relative_path) const = 0;

    void remove(const fs::path& rel_path, unsigned int userId) const;

    [[nodiscard]] engine::StorageType type() const override = 0;

protected:
    void removeFile(const fs::path& rel_path, unsigned int userId) const;
    void removeDirectory(const fs::path& rel_path, unsigned int userId) const;
};

} // namespace vh::storage
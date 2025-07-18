#pragma once

#include "storage/StorageEngine.hpp"
#include "types/APIKey.hpp"
#include "cloud/S3Provider.hpp"

namespace vh::types {
struct File;
struct Directory;
struct S3Vault;
struct Sync;
struct CacheIndex;

namespace api {
struct APIKey;
}}

namespace vh::concurrency {
class SyncTask;
}

namespace vh::services {
class SyncController;
}

namespace vh::storage {

class CloudStorageEngine : public StorageEngine {
public:
    std::shared_ptr<types::Sync> sync;

    CloudStorageEngine() = default;

    ~CloudStorageEngine() override = default;

    CloudStorageEngine(const std::shared_ptr<concurrency::ThumbnailWorker>& thumbnailWorker,
                       const std::shared_ptr<types::S3Vault>& vault,
                       const std::shared_ptr<types::api::APIKey>& key,
                       const std::shared_ptr<types::Sync>& sync);

    void finishUpload(unsigned int userId, const std::filesystem::path& relPath) override;

    void mkdir(const fs::path& relative_path) override;

    [[nodiscard]] StorageType type() const override { return StorageType::Cloud; }

    [[nodiscard]] std::optional<std::vector<uint8_t> > readFile(const std::filesystem::path& rel_path) const override;

    void remove(const std::filesystem::path& rel_path, unsigned int userId) override;

    void purge(const std::filesystem::path& rel_path) const;

    void removeLocally(const std::filesystem::path& rel_path) const;

    void removeRemotely(const std::filesystem::path& rel_path, bool rmThumbnails = true) const;

    [[nodiscard]] bool fileExists(const std::filesystem::path& rel_path) const override;

    [[nodiscard]] bool isDirectory(const fs::path& rel_path) const override;

    [[nodiscard]] bool isFile(const fs::path& rel_path) const override;

    void uploadFile(const std::filesystem::path& rel_path) const;
    std::shared_ptr<types::File> downloadFile(const std::filesystem::path& rel_path);
    void indexAndDeleteFile(const std::filesystem::path& rel_path);

    [[nodiscard]] std::string getRemoteContentHash(const std::filesystem::path& rel_path) const;

    [[nodiscard]] std::unordered_map<std::u8string, std::shared_ptr<types::File>> getGroupedFilesFromS3(const std::filesystem::path& prefix = {}) const;

    std::vector<std::shared_ptr<types::Directory>> extractDirectories(const std::vector<std::shared_ptr<types::File>>& files) const;

protected:
    void removeFile(const fs::path& rel_path, unsigned int userId) override;
    void removeDirectory(const fs::path& rel_path, unsigned int userId) override;

private:
    std::shared_ptr<types::api::APIKey> key_;
    std::shared_ptr<cloud::S3Provider> s3Provider_;

    std::string downloadToBuffer(const std::filesystem::path& rel_path) const;
};

std::string getMimeType(const std::filesystem::path& path);
std::u8string stripLeadingSlash(const std::filesystem::path& path);

} // namespace vh::storage
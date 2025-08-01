#pragma once

#include "storage/StorageEngine.hpp"

#include <unordered_map>

namespace vh::types {
struct File;
struct Directory;
struct S3Vault;

namespace api {
struct APIKey;
}}

namespace vh::cloud {
class S3Provider;
}

namespace vh::storage {

class CloudStorageEngine : public StorageEngine {
public:
    CloudStorageEngine() = default;
    ~CloudStorageEngine() override = default;

    explicit CloudStorageEngine(const std::shared_ptr<types::S3Vault>& vault);

    [[nodiscard]] StorageType type() const override { return StorageType::Cloud; }

    void purge(const std::filesystem::path& rel_path) const;

    void removeLocally(const std::filesystem::path& rel_path) const;

    void removeRemotely(const std::filesystem::path& rel_path, bool rmThumbnails = true) const;

    void uploadFile(const std::filesystem::path& rel_path) const;

    std::shared_ptr<types::File> downloadFile(const std::filesystem::path& rel_path);

    void indexAndDeleteFile(const std::filesystem::path& rel_path);

    [[nodiscard]] std::string getRemoteContentHash(const std::filesystem::path& rel_path) const;

    [[nodiscard]] std::unordered_map<std::u8string, std::shared_ptr<types::File>> getGroupedFilesFromS3(const std::filesystem::path& prefix = {}) const;

    std::vector<std::shared_ptr<types::Directory>> extractDirectories(const std::vector<std::shared_ptr<types::File>>& files) const;

private:
    std::shared_ptr<types::api::APIKey> key_;
    std::shared_ptr<cloud::S3Provider> s3Provider_;

    std::vector<uint8_t> downloadToBuffer(const std::filesystem::path& rel_path) const;

    [[nodiscard]] bool remoteFileIsEncrypted(const std::filesystem::path& rel_path) const;

    std::optional<std::string> getRemoteIVBase64(const std::filesystem::path& rel_path) const;
};

} // namespace vh::storage
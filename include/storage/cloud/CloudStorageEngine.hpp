#pragma once

#include "storage/StorageEngine.hpp"

#include <unordered_map>
#include <memory>
#include <vector>

namespace vh::types {
struct File;
struct Directory;
struct S3Vault;
struct TrashedFile;

namespace api {
struct APIKey;
}

namespace sync {
struct RemotePolicy;
}

}

namespace vh::cloud {
class S3Controller;
}

namespace vh::storage {

class CloudStorageEngine final : public StorageEngine {
public:
    CloudStorageEngine() = default;
    ~CloudStorageEngine() override = default;

    explicit CloudStorageEngine(const std::shared_ptr<types::S3Vault>& vault);

    [[nodiscard]] StorageType type() const override { return StorageType::Cloud; }

    void purge(const std::filesystem::path& rel_path) const;
    void purge(const std::shared_ptr<types::TrashedFile>& f) const;

    void removeLocally(const std::filesystem::path& rel_path) const;
    void removeLocally(const std::shared_ptr<types::TrashedFile>& f) const;

    void removeRemotely(const std::filesystem::path& rel_path, bool rmThumbnails = true) const;
    void removeRemotely(const std::shared_ptr<types::TrashedFile>& f, bool rmThumbnails = true) const;

    void upload(const std::shared_ptr<types::File>& f) const;

    void upload(const std::shared_ptr<types::File>& f, const std::vector<uint8_t>& buffer, bool isCiphertext = true) const;

    std::shared_ptr<types::File> downloadFile(const std::filesystem::path& rel_path);

    std::vector<uint8_t> downloadToBuffer(const std::filesystem::path& rel_path) const;

    void indexAndDeleteFile(const std::filesystem::path& rel_path);

    [[nodiscard]] std::string getRemoteContentHash(const std::filesystem::path& rel_path) const;

    [[nodiscard]] std::unordered_map<std::u8string, std::shared_ptr<types::File>> getGroupedFilesFromS3(const std::filesystem::path& prefix = {}) const;

    std::vector<std::shared_ptr<types::Directory>> extractDirectories(const std::vector<std::shared_ptr<types::File>>& files) const;

    [[nodiscard]] bool remoteFileIsEncrypted(const std::filesystem::path& rel_path) const;

    std::optional<std::pair<std::string, unsigned int>> getRemoteIVBase64AndVersion(const std::filesystem::path& rel_path) const;

    std::shared_ptr<types::sync::RemotePolicy> remote_policy() const;

private:
    std::shared_ptr<types::api::APIKey> key_;
    std::shared_ptr<cloud::S3Controller> s3Provider_;

    std::shared_ptr<types::S3Vault> s3Vault() const;

    std::unordered_map<std::string, std::string> getMetaMapFromFile(const std::shared_ptr<types::File>& f) const;
};

} // namespace vh::storage
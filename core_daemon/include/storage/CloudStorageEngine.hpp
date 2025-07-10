#pragma once

#include "storage/StorageEngine.hpp"
#include "types/APIKey.hpp"
#include "cloud/S3Provider.hpp"

namespace vh::types {
struct File;
struct S3Vault;

namespace api {
struct APIKey;
}

}

namespace vh::storage {

class CloudStorageEngine : public StorageEngine {
public:
    CloudStorageEngine() = default;

    ~CloudStorageEngine() override = default;

    CloudStorageEngine(const std::shared_ptr<types::S3Vault>& vault,
                       const std::shared_ptr<types::api::APIKey>& key);

    void mkdir(const fs::path& relative_path) override;

    [[nodiscard]] StorageType type() const override { return StorageType::Cloud; }

    [[nodiscard]] std::optional<std::vector<uint8_t> > readFile(const std::filesystem::path& rel_path) const override;

    bool deleteFile(const std::filesystem::path& rel_path) override;

    [[nodiscard]] bool fileExists(const std::filesystem::path& rel_path) const override;

    [[nodiscard]] std::filesystem::path getAbsolutePath(const std::filesystem::path& rel_path) const override;

    void uploadFile(const std::filesystem::path& rel_path, bool overwrite = false);

    void initCloudStorage() const;

private:
    unsigned int cache_expiry_days_;
    std::shared_ptr<types::api::APIKey> key_;
    std::shared_ptr<cloud::S3Provider> s3Provider_;
};

std::string getMimeType(const std::filesystem::path& path);

} // namespace vh::storage
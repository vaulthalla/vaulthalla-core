#pragma once

#include "util/curlWrappers.hpp"

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <unordered_map>

namespace vh::types::api {
struct APIKey;
}

namespace vh::cloud {

struct ValidateResult { bool ok; std::string msg; };

class S3Controller {
public:
    S3Controller(const std::shared_ptr<types::api::APIKey>& apiKey, std::string  bucket);

    ~S3Controller();

    void uploadObject(const std::filesystem::path& key, const std::filesystem::path& filePath) const;

    void uploadBufferWithMetadata(
        const std::filesystem::path& key,
        const std::vector<uint8_t>& buffer,
        const std::unordered_map<std::string, std::string>& metadata) const;

    void downloadObject(const std::filesystem::path& key, const std::filesystem::path& outputPath) const;

    void deleteObject(const std::filesystem::path& path) const;

    void uploadLargeObject(const std::filesystem::path& key,
                           const std::filesystem::path& filePath,
                           size_t partSize = 5 * 1024 * 1024) const;

    [[nodiscard]] std::string initiateMultipartUpload(const std::filesystem::path& key) const;

    void uploadPart(const std::filesystem::path& key, const std::string& uploadId,
                    int partNumber, const std::string& partData, std::string& etagOut) const;

    void completeMultipartUpload(const std::filesystem::path& key, const std::string& uploadId,
                                 const std::vector<std::string>& etags) const;

    void abortMultipartUpload(const std::filesystem::path& key, const std::string& uploadId) const;

    [[nodiscard]] std::u8string listObjects(const std::filesystem::path& prefix = {}) const;

    [[nodiscard]] std::optional<std::unordered_map<std::string, std::string>> getHeadObject(const std::filesystem::path& key) const;

    void setObjectContentHash(const std::filesystem::path& key, const std::string& hash) const;

    void setObjectEncryptionMetadata(const std::string& key, const std::string& iv_b64, unsigned int key_version) const;

    void downloadToBuffer(const std::filesystem::path& key, std::vector<uint8_t>& outBuffer) const;

    [[nodiscard]] ValidateResult validateAPICredentials() const;

    [[nodiscard]] bool isBucketEmpty(const std::string& bucket) const;

private:
    std::shared_ptr<types::api::APIKey> apiKey_;
    std::string bucket_;

    [[nodiscard]] std::map<std::string, std::string> buildHeaderMap(const std::string& payloadHash) const;

    std::pair<std::string, std::string> constructPaths(CURL* curl, const std::filesystem::path& p,
                                                       const std::string& query = "") const;

    [[nodiscard]] SList makeSigHeaders(const std::string& method,
                                       const std::string& canonical,
                                       const std::string& payloadHash) const;

};

}
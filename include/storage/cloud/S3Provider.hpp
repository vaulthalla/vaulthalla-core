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
struct S3APIKey;
}

namespace vh::cloud {

class S3Provider {
public:
    S3Provider(const std::shared_ptr<types::api::S3APIKey>& apiKey, const std::string& bucket);

    ~S3Provider();

    bool uploadObject(const std::filesystem::path& key, const std::filesystem::path& filePath) const;

    bool uploadBufferWithMetadata(
        const std::filesystem::path& key,
        const std::vector<uint8_t>& buffer,
        const std::unordered_map<std::string, std::string>& metadata) const;

    bool downloadObject(const std::filesystem::path& key, const std::filesystem::path& outputPath) const;

    bool deleteObject(const std::filesystem::path& path) const;

    bool uploadLargeObject(const std::filesystem::path& key,
                           const std::filesystem::path& filePath,
                           size_t partSize = 5 * 1024 * 1024) const;

    std::string initiateMultipartUpload(const std::filesystem::path& key) const;

    bool uploadPart(const std::filesystem::path& key, const std::string& uploadId,
                    int partNumber, const std::string& partData, std::string& etagOut) const;

    bool completeMultipartUpload(const std::filesystem::path& key, const std::string& uploadId,
                                 const std::vector<std::string>& etags) const;

    bool abortMultipartUpload(const std::filesystem::path& key, const std::string& uploadId) const;

    std::u8string listObjects(const std::filesystem::path& prefix = {}) const;

    std::optional<std::unordered_map<std::string, std::string>> getHeadObject(const std::filesystem::path& key) const;

    bool setObjectContentHash(const std::filesystem::path& key, const std::string& hash) const;

    bool setObjectEncryptionMetadata(const std::string& key, const std::string& iv_b64) const;

    bool downloadToBuffer(const std::filesystem::path& key, std::vector<uint8_t>& outBuffer) const;

private:
    std::shared_ptr<types::api::S3APIKey> apiKey_;
    std::string bucket_;

    std::map<std::string, std::string> buildHeaderMap(const std::string& payloadHash) const;

    std::pair<std::string, std::string> constructPaths(CURL* curl, const std::filesystem::path& p,
                                                       const std::string& query = "") const;

    SList makeSigHeaders(const std::string& method,
                         const std::string& canonical,
                         const std::string& payloadHash) const;

};

}
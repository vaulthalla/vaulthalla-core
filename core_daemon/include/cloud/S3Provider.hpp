#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vh::types::api {
class S3APIKey;
}

namespace vh::cloud {
class S3Provider {
  public:
    S3Provider(const std::shared_ptr<types::api::S3APIKey>& apiKey, const std::string& bucket);
    ~S3Provider();

    bool uploadObject(const std::string& key, const std::string& filePath);
    bool downloadObject(const std::string& key, const std::string& outputPath);
    bool deleteObject(const std::string& key);

    bool uploadLargeObject(const std::string& key, const std::string& filePath,
                           size_t partSize = 5 * 1024 * 1024);

    std::string initiateMultipartUpload(const std::string& key);
    bool uploadPart(const std::string& key, const std::string& uploadId, int partNumber,
                    const std::string& partData, std::string& etagOut);
    bool completeMultipartUpload(const std::string& key, const std::string& uploadId,
                                 const std::vector<std::string>& etags);
    bool abortMultipartUpload(const std::string& key, const std::string& uploadId);

    std::string listObjects(const std::string& prefix = "");
    bool downloadToBuffer(const std::string& key, std::string& outBuffer);

  private:
    std::shared_ptr<types::api::S3APIKey> apiKey_;
    std::string bucket_;

    std::string buildAuthorizationHeader(const std::string& method, const std::string& canonicalUri,
                                         const std::map<std::string, std::string>& headers,
                                         const std::string& payloadHash);
    std::string sha256Hex(const std::string& data);
    std::string hmacSha256Hex(const std::string& key, const std::string& data);
};
} // namespace vh::cloud::s3

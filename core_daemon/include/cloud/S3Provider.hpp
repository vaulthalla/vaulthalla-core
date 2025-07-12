#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <curl/curl.h>

namespace vh::types::api {
class S3APIKey;
}

namespace vh::cloud {
class S3Provider {
  public:
    S3Provider(const std::shared_ptr<types::api::S3APIKey>& apiKey, const std::string& bucket);
    ~S3Provider();

    bool uploadObject(const std::filesystem::path& key, const std::filesystem::path& filePath);
    bool downloadObject(const std::filesystem::path& key, const std::filesystem::path& outputPath);
    bool deleteObject(const std::filesystem::path& path);

    bool uploadLargeObject(const std::string& key, const std::string& filePath,
                           size_t partSize = 5 * 1024 * 1024);

    std::string initiateMultipartUpload(const std::string& key);
    bool uploadPart(const std::string& key, const std::string& uploadId, int partNumber,
                    const std::string& partData, std::string& etagOut);
    bool completeMultipartUpload(const std::string& key, const std::string& uploadId,
                                 const std::vector<std::string>& etags);
    bool abortMultipartUpload(const std::string& key, const std::string& uploadId);

    std::u8string listObjects(const std::filesystem::path& prefix = {});
    bool downloadToBuffer(const std::string& key, std::string& outBuffer);

  private:
    std::shared_ptr<types::api::S3APIKey> apiKey_;
    std::string bucket_;

    std::string buildAuthorizationHeader(const std::string& method, const std::string& fullPath,
                                         const std::map<std::string, std::string>& headers,
                                         const std::string& payloadHash);
    std::string sha256Hex(const std::string& data);
    std::string hmacSha256Hex(const std::string& key, const std::string& data);

    std::string getHost() const;
    std::map<std::string, std::string> buildHeaderMap(const std::string& payloadHash) const;
    std::string escapeKeyPreserveSlashes(CURL* curl, const std::filesystem::path& p) const;
    std::pair<std::string, std::string> constructPaths(CURL* curl, const std::filesystem::path& p, const std::string& query = "") const;

    static size_t writeToString(char* ptr, size_t size, size_t nmemb, void* userdata);

};

} // namespace vh::cloud::s3

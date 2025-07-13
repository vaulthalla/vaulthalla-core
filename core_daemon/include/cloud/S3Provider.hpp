#pragma once

#include "util/curlWrappers.hpp"

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

    bool uploadObject(const std::filesystem::path& key, const std::filesystem::path& filePath) const;

    bool downloadObject(const std::filesystem::path& key, const std::filesystem::path& outputPath) const;

    bool deleteObject(const std::filesystem::path& path) const;

    bool uploadLargeObject(const std::string& key, const std::string& filePath,
                           size_t partSize = 5 * 1024 * 1024) const;

    std::string initiateMultipartUpload(const std::string& key) const;

    bool uploadPart(const std::string& key, const std::string& uploadId, int partNumber,
                    const std::string& partData, std::string& etagOut) const;

    bool completeMultipartUpload(const std::string& key, const std::string& uploadId,
                                 const std::vector<std::string>& etags) const;

    bool abortMultipartUpload(const std::string& key, const std::string& uploadId) const;

    std::u8string listObjects(const std::filesystem::path& prefix = {}) const;

    bool downloadToBuffer(const std::string& key, std::string& outBuffer) const;

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
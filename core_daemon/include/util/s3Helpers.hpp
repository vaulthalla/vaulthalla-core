#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <curl/curl.h>
#include <map>

namespace vh::types::api {
struct S3APIKey;
}

namespace vh::util {

std::string sha256Hex(const std::string& data);
std::string hmacSha256Hex(const std::string& key, const std::string& data);
std::string hmacSha256Raw(const std::string& key, const std::string& data);
std::string hmacSha256HexFromRaw(const std::string& rawKey, const std::string& data);
std::string escapeKeyPreserveSlashes(CURL* curl, const std::filesystem::path& p);
std::string composeMultiPartUploadXMLBody(const std::vector<std::string>& etags);
size_t writeToString(const char* ptr, size_t size, size_t nmemb, void* userdata);
void parsePagination(const std::string& response, std::string& continuationToken, bool& moreResults);
[[nodiscard]] bool extractETag(const std::string& respHdr, std::string& etagOut);
std::string buildAuthorizationHeader(const std::shared_ptr<types::api::S3APIKey>& api_key,
                                     const std::string& method, const std::string& fullPath,
                                     const std::map<std::string, std::string>& headers,
                                     const std::string& payloadHash);

}

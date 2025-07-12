#include "cloud/S3Provider.hpp"
#include "types/APIKey.hpp"
#include "shared_util/timestamp.hpp"
#include "shared_util/u8.hpp"
#include "util/s3Helpers.hpp"

#include <ctime>
#include <curl/curl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <regex>
#include <sstream>
#include <vector>

using namespace vh::cloud;

// --- helpers --------------------------------------------------------------
namespace {
/** Ensure curl global init runs exactly once in process */
void ensureCurlGlobalInit() {
    static std::once_flag once;
    std::call_once(once, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
}

/** Build a curl_slist from stable heap‑stored strings */
struct HeaderList {
    std::vector<std::string> store; // owns the memory
    curl_slist* list = nullptr;     // raw list pointer
    ~HeaderList() { curl_slist_free_all(list); }

    void add(const std::string& h) {
        store.push_back(h);
        list = curl_slist_append(list, store.back().c_str());
    }
};
} // namespace

// --- ctor/dtor ------------------------------------------------------------
S3Provider::S3Provider(const std::shared_ptr<types::api::S3APIKey>& apiKey, const std::string& bucket)
: apiKey_(apiKey), bucket_(bucket) {
    if (!apiKey_) throw std::runtime_error("S3Provider requires a valid S3APIKey");
    ensureCurlGlobalInit();
}

S3Provider::~S3Provider() = default;

// -------------------------------------------------------------------------
// uploadObject / downloadObject / deleteObject
// -------------------------------------------------------------------------
bool S3Provider::uploadObject(const std::filesystem::path& key, const std::filesystem::path& filePath) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::ifstream file(filePath, std::ios::binary);
    if (!file) return false;

    const auto startPos = file.tellg();
    file.seekg(0, std::ios::end);
    const curl_off_t fileSize = file.tellg();
    file.seekg(startPos);

    const auto [canonicalPath, url] = constructPaths(curl, key);

    std::ostringstream bodyHashStream;
    bodyHashStream << file.rdbuf();
    const std::string payloadHash = util::sha256Hex(bodyHashStream.str());
    file.clear(); file.seekg(startPos);

    auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader("PUT", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Content-Type: application/octet-stream");
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    auto readFn = +[](char* buf, size_t sz, size_t nm, void* ud) -> size_t {
        auto* fin = static_cast<std::ifstream*>(ud);
        fin->read(buf, static_cast<std::streamsize>(sz * nm));
        return static_cast<size_t>(fin->gcount());
    };

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readFn);
    curl_easy_setopt(curl, CURLOPT_READDATA, &file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, fileSize);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

bool S3Provider::downloadObject(const std::filesystem::path& key,
                                const std::filesystem::path& outputPath) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::ofstream file(outputPath, std::ios::binary);
    if (!file) {
        curl_easy_cleanup(curl);
        return false;
    }

    const auto [canonicalPath, url] = constructPaths(curl, key);

    const std::string payloadHash = "UNSIGNED-PAYLOAD";
    auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader("GET", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    auto writeFn = +[](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
        std::ofstream* fout = static_cast<std::ofstream*>(userdata);
        fout->write(ptr, size * nmemb);
        return size * nmemb;
    };

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    file.close();
    return res == CURLE_OK;
}

bool S3Provider::deleteObject(const std::filesystem::path& path) {
    CURL* curl = curl_easy_init();
    if (!curl) throw std::runtime_error("Failed to initialize cURL");

    std::cout << "[S3Provider] Deleting object: " << to_utf8_string(path.u8string()) << std::endl;

    const auto [canonicalPath, url] = constructPaths(curl, path);

    const std::string payloadHash = util::sha256Hex("");
    const auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader("DELETE", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);

    std::string responseBody;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, util::writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

    long httpCode = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::cerr << "[S3Provider] curl error: " << curl_easy_strerror(res) << "\n";
        return false;
    }

    if (httpCode >= 200 && httpCode < 300) return true;

    std::cerr << "[S3Provider] DELETE " << canonicalPath << " failed (HTTP " << httpCode << "):\n" << responseBody << "\n";
    return false;
}

std::string S3Provider::buildAuthorizationHeader(const std::string& method,
                                                 const std::string& fullPath,
                                                 const std::map<std::string, std::string>& headers,
                                                 const std::string& payloadHash) {
    std::string canonicalPath, canonicalQuery;
    const auto qpos = fullPath.find('?');
    if (qpos == std::string::npos) canonicalPath = fullPath;
    else {
        canonicalPath = fullPath.substr(0, qpos);
        canonicalQuery = fullPath.substr(qpos + 1);
        if (canonicalQuery.find('=') == std::string::npos) canonicalQuery += '=';
    }

    const std::string service = "s3";
    const std::string algorithm = "AWS4-HMAC-SHA256";
    const std::string amzDate = headers.at("x-amz-date");
    const std::string dateStamp = util::getDate();

    // Headers
    std::string canonicalHeaders, signedHeaders;
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        canonicalHeaders += it->first + ":" + it->second + "\n";
        signedHeaders += it->first;
        if (std::next(it) != headers.end()) signedHeaders += ";";
    }

    // Canonical Request
    std::ostringstream canonicalRequestStream;
    canonicalRequestStream << method << "\n"
                           << canonicalPath << "\n"
                           << canonicalQuery << "\n"
                           << canonicalHeaders << "\n"
                           << signedHeaders << "\n"
                           << payloadHash;

    const std::string canonicalRequest = canonicalRequestStream.str();
    const std::string hashedCanonicalRequest = util::sha256Hex(canonicalRequest);

    // String to Sign
    const std::string credentialScope = dateStamp + "/" + apiKey_->region + "/" + service + "/aws4_request";
    std::ostringstream stringToSignStream;
    stringToSignStream << algorithm << "\n" << amzDate << "\n" << credentialScope << "\n" << hashedCanonicalRequest;
    const std::string stringToSign = stringToSignStream.str();

    // Signing key
    std::string kDate    = util::hmacSha256Raw("AWS4" + apiKey_->secret_access_key, dateStamp);
    std::string kRegion  = util::hmacSha256Raw(kDate, apiKey_->region);
    std::string kService = util::hmacSha256Raw(kRegion, service);
    std::string kSigning = util::hmacSha256Raw(kService, "aws4_request");

    // Signature
    const std::string signature = util::hmacSha256HexFromRaw(kSigning, stringToSign);

    // Final header
    std::ostringstream authHeader;
    authHeader << algorithm << " "
               << "Credential=" << apiKey_->access_key << "/" << credentialScope << ", "
               << "SignedHeaders=" << signedHeaders << ", "
               << "Signature=" << signature;

    return authHeader.str();
}

// -------------------------------------------------------------------------
// Multipart helpers (initiate/uploadPart/complete/abort)
// Header lifetimes fixed + POSTFIELDSIZE_LARGE set
// -------------------------------------------------------------------------
std::string S3Provider::initiateMultipartUpload(const std::string& key) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    const std::string rawKey = key;
    const auto [canonicalPath, url] = constructPaths(curl, key, "?uploads");
    const std::string payloadHash = "UNSIGNED-PAYLOAD";

    auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader("POST", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)0);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, util::writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        std::cerr << "[S3Provider] initiateMultipartUpload failed: CURL=" << res
                  << " HTTP=" << httpCode << "\nResp: " << response << std::endl;
        return "";
    }

    std::smatch m;
    std::regex re(R"(<UploadId>([^<]+)</UploadId>)");
    if (std::regex_search(response, m, re) && m.size() > 1)
        return m[1].str();

    std::cerr << "[S3Provider] Failed to parse UploadId from response:\n" << response << std::endl;
    return "";
}

bool S3Provider::uploadPart(const std::string& key, const std::string& uploadId,
                            const int partNumber, const std::string& partData, std::string& etagOut) {
    std::cout << "Uploading part " << partNumber << " for " << key << " to bucket " << bucket_ << std::endl;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    // Compose URL and Canonical Path
    const std::string query = "?partNumber=" + std::to_string(partNumber) + "&uploadId=" + uploadId;
    const auto [canonicalPath, url] = constructPaths(curl, key, query);

    const std::string payloadHash = util::sha256Hex(partData);
    const auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader("PUT", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Content-Type: application/octet-stream");
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    std::string respHdr;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, partData.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(partData.size()));
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, util::writeToString);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &respHdr);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) return false;

    return util::extractETag(respHdr, etagOut);
}

bool S3Provider::completeMultipartUpload(const std::string& key,
                                         const std::string& uploadId,
                                         const std::vector<std::string>& etags) {
    if (etags.empty()) return false;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    // Build URI and Canonical Path
    const std::string query = "?uploadId=" + uploadId;
    const auto [canonicalPath, url] = constructPaths(curl, key, query);

    const auto body = util::composeMultiPartUploadXMLBody(etags);

    const std::string payloadHash = util::sha256Hex(body);
    auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader("POST", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Content-Type: application/xml");
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, util::writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        std::cerr << "[S3Provider] completeMultipartUpload failed: CURL=" << res
                  << " HTTP=" << httpCode << "\nResponse:\n" << response << std::endl;
        return false;
    }

    return true;
}

bool S3Provider::abortMultipartUpload(const std::string& key, const std::string& uploadId) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    const std::string query = "?uploadId=" + uploadId;
    const auto [canonicalPath, url] = constructPaths(curl, key, query);

    const std::string payloadHash = util::sha256Hex("");
    const auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader("DELETE", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}

// -------------------------------------------------------------------------
// uploadLargeObject unchanged (uses safe uploadPart above)
// -------------------------------------------------------------------------
bool S3Provider::uploadLargeObject(const std::string& key,
                                   const std::string& filePath,
                                   size_t partSize) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return false;

    const std::string uploadId = initiateMultipartUpload(key);
    if (uploadId.empty()) return false;

    std::vector<std::string> etags;
    int partNo = 1;
    bool success = true;

    while (file && success) {
        std::string part(partSize, '\0');
        file.read(&part[0], static_cast<std::streamsize>(partSize));
        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) break;

        part.resize(static_cast<size_t>(bytesRead));  // trim excess buffer

        std::string etag;
        success = uploadPart(key, uploadId, partNo++, part, etag);
        if (success) etags.push_back(std::move(etag));
    }

    if (!success || etags.empty()) {
        abortMultipartUpload(key, uploadId);
        return false;
    }

    return completeMultipartUpload(key, uploadId, etags);
}

std::u8string S3Provider::listObjects(const std::filesystem::path& prefix) {
    std::u8string fullXmlResponse;
    std::string continuationToken;
    bool moreResults = true;

    while (moreResults) {
        CURL* curl = curl_easy_init();
        if (!curl) break;

        std::string escapedPrefix;
        if (!prefix.empty()) escapedPrefix = util::escapeKeyPreserveSlashes(curl, prefix);

        std::ostringstream uri;
        uri << "/" << bucket_ << "?list-type=2";
        if (!escapedPrefix.empty()) uri << "&prefix=" << escapedPrefix;
        if (!continuationToken.empty()) {
            char* escapedToken = curl_easy_escape(curl, continuationToken.c_str(), static_cast<int>(continuationToken.size()));
            if (!escapedToken) {
                curl_easy_cleanup(curl);
                break;
            }
            uri << "&continuation-token=" << escapedToken;
            curl_free(escapedToken);
        }

        const std::string uriStr = uri.str();
        const std::string url = apiKey_->endpoint + uriStr;

        const std::string payloadHash = "UNSIGNED-PAYLOAD";
        const auto hdrMap = buildHeaderMap(payloadHash);
        const std::string authHeader = buildAuthorizationHeader("GET", uriStr, hdrMap, payloadHash);

        HeaderList headers;
        headers.add("Authorization: " + authHeader);
        for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, util::writeToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        const CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            std::cerr << "[S3Provider] listObjects failed: " << curl_easy_strerror(res) << std::endl;
            break;
        }

        // Append raw XML as UTF-8
        fullXmlResponse += std::u8string(reinterpret_cast<const char8_t*>(response.data()), response.size());

        util::parsePagination(response, continuationToken, moreResults);
    }

    return fullXmlResponse;
}

bool S3Provider::downloadToBuffer(const std::string& key, std::string& outBuffer) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    const auto [canonicalPath, url] = constructPaths(curl, key);
    const std::string payloadHash = "UNSIGNED-PAYLOAD";
    const auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader("GET", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    outBuffer.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, std::string* data) {
        data->append(ptr, size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outBuffer);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        std::cerr << "[S3Provider] downloadToBuffer failed: " << curl_easy_strerror(res)
                  << "\nKey: " << key << std::endl;

    return res == CURLE_OK;
}

std::map<std::string, std::string> S3Provider::buildHeaderMap(const std::string& payloadHash) const {
    return {
            {"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
            {"x-amz-content-sha256", payloadHash},
            {"x-amz-date", util::getCurrentTimestamp()}
    };
}

std::pair<std::string, std::string> S3Provider::constructPaths(CURL* curl, const std::filesystem::path& p, const std::string& query) const {
    const auto escapedKey = util::escapeKeyPreserveSlashes(curl, p);
    const auto canonicalPath = "/" + bucket_ + "/" + escapedKey + query;
    const auto url = apiKey_->endpoint + canonicalPath;
    return {canonicalPath, url};
}


#include "storage/cloud/S3Controller.hpp"
#include "types/APIKey.hpp"
#include "util/timestamp.hpp"
#include "util/u8.hpp"
#include "util/s3Helpers.hpp"
#include "services/LogRegistry.hpp"

#include <ctime>
#include <curl/curl.h>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <regex>
#include <sstream>
#include <utility>
#include <vector>

using namespace vh::cloud;
using namespace vh::types;
using namespace vh::util;
using namespace vh::logging;

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
    ~HeaderList() { if (list) curl_slist_free_all(list); }

    void add(const std::string& h) {
        store.push_back(h);
        list = curl_slist_append(list, store.back().c_str());
    }
};
} // namespace

// --- ctor/dtor ------------------------------------------------------------
S3Controller::S3Controller(const std::shared_ptr<api::APIKey>& apiKey, std::string bucket)
: apiKey_(apiKey), bucket_(std::move(bucket)) {
    if (!apiKey_) throw std::runtime_error("S3Provider requires a valid S3APIKey");
    ensureCurlGlobalInit();
}

S3Controller::~S3Controller() = default;

// -------------------------------------------------------------------------
// uploadObject / downloadObject / deleteObject
// -------------------------------------------------------------------------

inline std::string slurp(const std::istream& in) {
    std::ostringstream oss;
    oss << in.rdbuf();          // copy entire buffer
    return oss.str();
}

ValidateResult S3Controller::validateAPICredentials() const {
    if (apiKey_->secret_access_key.empty())
        throw std::runtime_error("S3Provider: API key secret is empty, cannot validate credentials");

    const std::regex re_key("^[A-Za-z0-9/+=]{20,128}$");
    const std::regex re_endpoint(R"(^https?://([A-Za-z0-9.-]+|\d{1,3}(?:\.\d{1,3}){3})(:\d{1,5})?/?$)");

    std::string errors;
    if (!std::regex_match(apiKey_->access_key, re_key))
        errors += "Access key format looks wrong (expect 20-128 alphanumeric chars, slashes, pluses, or equals).\n";
    if (!std::regex_match(apiKey_->secret_access_key, re_key))
        errors += "Secret access key format looks wrong (expect 20-128 alphanumeric chars, slashes, pluses, or equals).\n";
    if (!std::regex_match(apiKey_->endpoint, re_endpoint))
        errors += "Endpoint format looks wrong (expect https://<host>[:port]/).\n";

    if (!errors.empty())
        return {false, errors};

    // --- Live probe: ListBuckets ---
    const auto rstrip = [](std::string s) {
        while (!s.empty() && s.back() == '/') s.pop_back();
        return s;
    };

    const auto serviceUrl = rstrip(apiKey_->endpoint) + "/";

    static const std::string kCanonical = "/";
    static const std::string kUnsigned = "UNSIGNED-PAYLOAD";

    CurlEasy tmpHandle;
    SList hdrs = makeSigHeaders("GET", kCanonical, kUnsigned);
    hdrs.add("Content-Type: application/xml");

    const auto resp = performCurl([&](CURL* h) {
        curl_easy_setopt(h, CURLOPT_URL, serviceUrl.c_str());
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs.get());
        curl_easy_setopt(h, CURLOPT_HTTPGET, 1L);
        // Make sure no upload/body options are set for this handle
        curl_easy_setopt(h, CURLOPT_UPLOAD, 0L);
        curl_easy_setopt(h, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(0));
    });

    if (resp.ok()) return {true, "Credentials validated (ListBuckets succeeded)."};

    const std::string& body = resp.body;
    const bool accessDenied = body.find("AccessDenied") != std::string::npos;
    const bool badSig =
        body.find("SignatureDoesNotMatch") != std::string::npos ||
        body.find("InvalidAccessKeyId") != std::string::npos ||
        body.find("AuthFailure") != std::string::npos ||
        body.find("XAmzContentSHA256Mismatch") != std::string::npos;

    if (accessDenied && !badSig) return {true, "Credentials validated (auth OK, ListBuckets denied)."};
    return {false, "Auth probe failed: " + resp.body};
}

bool S3Controller::uploadObject(const std::filesystem::path& key, const std::filesystem::path& filePath) const {
    std::ifstream fin(filePath, std::ios::binary);
    if (!fin) return false;

    fin.seekg(0, std::ios::end);
    const curl_off_t sz = fin.tellg();
    fin.seekg(0);

    const std::string fileContents = slurp(fin);
    const std::string payloadHash = sha256Hex(fileContents);

    fin.clear();
    fin.seekg(0);

    CurlEasy tmpHandle;
    const auto [canonical, url] = constructPaths(static_cast<CURL*>(tmpHandle), key);

    SList hdrs = makeSigHeaders("PUT", canonical, payloadHash);
    hdrs.add("Content-Type: application/octet-stream");

    HttpResponse resp = performCurl([&](CURL* h) {
        curl_easy_setopt(h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(h, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs.get());
        curl_easy_setopt(h, CURLOPT_READDATA, &fin);
        curl_easy_setopt(h, CURLOPT_INFILESIZE_LARGE, sz);
        curl_easy_setopt(h, CURLOPT_READFUNCTION,
            +[](char* buf, size_t sz, size_t nm, void* ud) -> size_t {
                auto* fp = static_cast<std::ifstream*>(ud);
                fp->read(buf, static_cast<std::streamsize>(sz * nm));
                return static_cast<size_t>(fp->gcount());
            });
    });

    return resp.ok();
}

bool S3Controller::uploadBufferWithMetadata(
    const std::filesystem::path& key,
    const std::vector<uint8_t>& buffer,
    const std::unordered_map<std::string, std::string>& metadata) const
{
    const std::string payloadHash = sha256Hex({buffer.begin(), buffer.end()});

    CurlEasy tmpHandle;
    const auto [canonical, url] = constructPaths(static_cast<CURL*>(tmpHandle), key);

    SList hdrs = makeSigHeaders("PUT", canonical, payloadHash);
    hdrs.add("Content-Type: application/octet-stream");

    // Add x-amz-meta-* headers
    for (const auto& [k, v] : metadata)
        hdrs.add("x-amz-meta-" + k + ": " + v);

    HttpResponse resp = performCurl([&](CURL* h) {
        curl_easy_setopt(h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(h, CURLOPT_UPLOAD, 1L);
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs.get());
        curl_easy_setopt(h, CURLOPT_READDATA, &buffer);
        curl_easy_setopt(h, CURLOPT_INFILESIZE_LARGE, static_cast<curl_off_t>(buffer.size()));
        curl_easy_setopt(h, CURLOPT_READFUNCTION,
            +[](char* out, size_t size, size_t nmemb, void* userdata) -> size_t {
                auto* src = static_cast<std::string*>(userdata);
                size_t to_copy = std::min(size * nmemb, src->size());
                std::memcpy(out, src->data(), to_copy);
                src->erase(0, to_copy);  // consume
                return to_copy;
            });
    });

    return resp.ok();
}

bool S3Controller::downloadObject(const std::filesystem::path& key,
                                const std::filesystem::path& outputPath) const {
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
    const std::string authHeader =  buildAuthorizationHeader(apiKey_, "GET", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    auto writeFn = +[](const char* ptr, const size_t size, const size_t nmemb, void* userdata) -> size_t {
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

bool S3Controller::deleteObject(const std::filesystem::path& key) const {
    const CurlEasy tmpHandle;
    const auto [canonical, url] = constructPaths(static_cast<CURL*>(tmpHandle), key);

    const std::string payloadHash = sha256Hex("");
    SList hdrs = makeSigHeaders("DELETE", canonical, payloadHash);

    HttpResponse resp = performCurl([&](CURL* h){
        curl_easy_setopt(h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs.get());
    });

    if (!resp.ok()) LogRegistry::cloud()->error(
        "[S3Provider] deleteObject failed: CURL={} HTTP={} Response:\n{}",
        resp.curl, resp.http, resp.body
    );

    return resp.ok();
}

std::string S3Controller::initiateMultipartUpload(const std::filesystem::path& key) const {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    const auto keyStr = key.u8string();
    const auto [canonicalPath, url] = constructPaths(curl, keyStr, "?uploads");
    const std::string payloadHash = "UNSIGNED-PAYLOAD";

    auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader(apiKey_, "POST", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, static_cast<curl_off_t>(0));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        LogRegistry::cloud()->error("[S3Provider] initiateMultipartUpload failed: CURL={} HTTP={} Response:\n{}",
                                    res, httpCode, response);
        return "";
    }

    std::smatch m;
    std::regex re(R"(<UploadId>([^<]+)</UploadId>)");
    if (std::regex_search(response, m, re) && m.size() > 1)
        return m[1].str();

    LogRegistry::cloud()->error("[S3Provider] initiateMultipartUpload failed to parse UploadId from response: {}", response);
    return "";
}

bool S3Controller::uploadPart(const std::filesystem::path& key, const std::string& uploadId,
                             const int partNumber, const std::string& partData, std::string& etagOut) const {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    const auto keyStr = key.u8string();
    const std::string query = "?partNumber=" + std::to_string(partNumber) + "&uploadId=" + uploadId;
    const auto [canonicalPath, url] = constructPaths(curl, keyStr, query);

    const std::string payloadHash = sha256Hex(partData);
    const auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader(apiKey_, "PUT", canonicalPath, hdrMap, payloadHash);

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
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &respHdr);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK && extractETag(respHdr, etagOut);
}

bool S3Controller::completeMultipartUpload(const std::filesystem::path& key, const std::string& uploadId,
                                         const std::vector<std::string>& etags) const {
    if (etags.empty()) return false;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::u8string keyStr = key.u8string();
    const std::string query = "?uploadId=" + uploadId;
    const auto [canonicalPath, url] = constructPaths(curl, keyStr, query);

    const auto body = composeMultiPartUploadXMLBody(etags);
    const std::string payloadHash = sha256Hex(body);
    auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader(apiKey_, "POST", canonicalPath, hdrMap, payloadHash);

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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        LogRegistry::cloud()->error("[S3Provider] completeMultipartUpload failed: CURL={} HTTP={} Response:\n{}",
                                    res, httpCode, response);
        return false;
    }

    return true;
}

bool S3Controller::abortMultipartUpload(const std::filesystem::path& key, const std::string& uploadId) const {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::u8string keyStr = key.u8string();
    const std::string query = "?uploadId=" + uploadId;
    const auto [canonicalPath, url] = constructPaths(curl, keyStr, query);

    const std::string payloadHash = sha256Hex("");
    const auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader(apiKey_, "DELETE", canonicalPath, hdrMap, payloadHash);

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

bool S3Controller::uploadLargeObject(const std::filesystem::path& key,
                                   const std::filesystem::path& filePath,
                                   size_t partSize) const {
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

        part.resize(static_cast<size_t>(bytesRead));

        std::string etag;
        success = uploadPart(key, uploadId, partNo++, part, etag);
        if (success) etags.push_back(std::move(etag));
    }

    if (!success || etags.empty()) {
        if (!abortMultipartUpload(key, uploadId))
            LogRegistry::cloud()->error(
                "[S3Provider] uploadLargeObject failed to abort multipart upload for {}: uploadId={}",
                key.string(), uploadId);
        return false;
    }

    return completeMultipartUpload(key, uploadId, etags);
}

std::u8string S3Controller::listObjects(const std::filesystem::path& prefix) const {
    std::u8string fullXmlResponse;
    std::string continuationToken;
    bool moreResults = true;

    while (moreResults) {
        CURL* curl = curl_easy_init();
        if (!curl) break;

        std::string escapedPrefix;
        if (!prefix.empty()) escapedPrefix = escapeKeyPreserveSlashes(curl, prefix);

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
        const std::string authHeader =  buildAuthorizationHeader(apiKey_, "GET", uriStr, hdrMap, payloadHash);

        HeaderList headers;
        headers.add("Authorization: " + authHeader);
        for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeToString);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        const CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            LogRegistry::cloud()->error("[S3Provider] listObjects failed: CURL={} Response:\n{}",
                                        res, response);
            break;
        }

        // Append raw XML as UTF-8
        fullXmlResponse += std::u8string(reinterpret_cast<const char8_t*>(response.data()), response.size());

        parsePagination(response, continuationToken, moreResults);
    }

    return fullXmlResponse;
}

std::optional<std::unordered_map<std::string, std::string>>
S3Controller::getHeadObject(const std::filesystem::path& key) const {
    const auto [canonicalPath, url] = constructPaths(nullptr, key);
    const std::string payloadHash = "UNSIGNED-PAYLOAD";

    auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader(apiKey_, "HEAD", canonicalPath, hdrMap, payloadHash);

    SList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    HttpResponse resp = performCurl([&](CURL* h) {
        curl_easy_setopt(h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(h, CURLOPT_NOBODY, 1L);            // HEAD request
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers.get());
    });

    if (!resp.ok()) {
        LogRegistry::cloud()->error("[S3Provider] getHeadObject failed for {}: CURL={} HTTP={}",
                                        key.string(), resp.curl, resp.http);
        return std::nullopt;
    }

    // Parse response headers into key-value pairs
    std::unordered_map<std::string, std::string> metadata;
    std::istringstream headerStream(resp.hdr);
    std::string line;
    while (std::getline(headerStream, line)) {
        if (auto pos = line.find(':'); pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            trimInPlace(key);
            trimInPlace(value);
            metadata.emplace(std::move(key), std::move(value));
        }
    }

    return metadata;
}

bool S3Controller::setObjectContentHash(const std::filesystem::path& key, const std::string& hash) const {
    CurlEasy curl;
    const auto [canonicalPath, url] = constructPaths(static_cast<CURL*>(curl), key);

    const std::string payloadHash = "UNSIGNED-PAYLOAD";

    auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader(apiKey_, "PUT", canonicalPath, hdrMap, payloadHash);

    SList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    std::ostringstream source;
    source << "/" << bucket_ << "/" << escapeKeyPreserveSlashes(static_cast<CURL*>(curl), key);

    headers.add("x-amz-copy-source: " + source.str());
    headers.add("x-amz-metadata-directive: REPLACE");
    headers.add("x-amz-meta-content-hash: " + hash);

    HttpResponse resp = performCurl([&](CURL* h) {
        curl_easy_setopt(h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers.get());
    });

    if (!resp.ok())
        LogRegistry::cloud()->error("[S3Provider] setObjectContentHash failed for {}: CURL={} HTTP={}",
                                        key.string(), resp.curl, resp.http);

    return resp.ok();
}

bool S3Controller::setObjectEncryptionMetadata(const std::string& key, const std::string& iv_b64) const {
    CurlEasy curl;
    const auto [canonicalPath, url] = constructPaths(static_cast<CURL*>(curl), key);
    const std::string payloadHash = "UNSIGNED-PAYLOAD";

    auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader(apiKey_, "PUT", canonicalPath, hdrMap, payloadHash);

    SList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    std::ostringstream source;
    source << "/" << bucket_ << "/" << escapeKeyPreserveSlashes(static_cast<CURL*>(curl), key);

    headers.add("x-amz-copy-source: " + source.str());
    headers.add("x-amz-metadata-directive: REPLACE");

    headers.add("x-amz-meta-vh-encrypted: true");
    headers.add("x-amz-meta-vh-iv: " + iv_b64);
    headers.add("x-amz-meta-vh-algo: aes256gcm");

    HttpResponse resp = performCurl([&](CURL* h) {
        curl_easy_setopt(h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers.get());
    });

    if (!resp.ok())
        LogRegistry::cloud()->error("[S3Provider] setObjectEncryptionMetadata failed for {}: CURL={} HTTP={}",
                                        key, resp.curl, resp.http);

    return resp.ok();
}

bool S3Controller::downloadToBuffer(const std::filesystem::path& key, std::vector<uint8_t>& outBuffer) const {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    const auto [canonicalPath, url] = constructPaths(curl, key);
    const std::string payloadHash = "UNSIGNED-PAYLOAD";
    const auto hdrMap = buildHeaderMap(payloadHash);
    const std::string authHeader = buildAuthorizationHeader(apiKey_, "GET", canonicalPath, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    outBuffer.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, std::vector<uint8_t>* data) {
        data->insert(data->end(), ptr, ptr + size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outBuffer);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
        LogRegistry::cloud()->error("[S3Provider] downloadToBuffer failed for {}: CURL={} Response:\n{}",
                                        key.string(), res, curl_easy_strerror(res));

    return res == CURLE_OK;
}

std::map<std::string, std::string> S3Controller::buildHeaderMap(const std::string& payloadHash) const {
    return {
            {"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
            {"x-amz-content-sha256", payloadHash},
            {"x-amz-date", getCurrentTimestamp()}
    };
}

std::pair<std::string, std::string> S3Controller::constructPaths(CURL* curl, const std::filesystem::path& p, const std::string& query) const {
    const auto escapedKey = escapeKeyPreserveSlashes(curl, p);
    const auto canonicalPath = "/" + bucket_ + "/" + escapedKey + query;
    const auto url = apiKey_->endpoint + canonicalPath;
    return {canonicalPath, url};
}

SList S3Controller::makeSigHeaders(const std::string& method,
                                 const std::string& canonical,
                                 const std::string& payloadHash) const {
    auto base = buildHeaderMap(payloadHash);      // host + dates
    const auto auth =  buildAuthorizationHeader(apiKey_, method, canonical, base, payloadHash);

    SList out;
    out.add("Authorization: " + auth);
    for (auto& [k, v] : base) out.add(k + ": " + v);
    return out;  // RAII slist
}

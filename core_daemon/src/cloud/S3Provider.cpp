// S3Provider.cpp — safe header lifetime + explicit payload sizes
#include "cloud/S3Provider.hpp"
#include "../../../shared/include/types/APIKey.hpp"
#include "../../../shared/include/shared_util/timestamp.hpp"
#include <ctime>
#include <curl/curl.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <regex>
#include <sstream>
#include <vector>

using namespace vh::cloud::s3;

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
S3Provider::S3Provider(const std::shared_ptr<types::api::S3APIKey>& apiKey) : apiKey_(apiKey) {
    if (!apiKey_) throw std::runtime_error("S3Provider requires a valid S3APIKey");
    ensureCurlGlobalInit();
}

S3Provider::~S3Provider() = default;

// -------------------------------------------------------------------------
// uploadObject / downloadObject / deleteObject
// -------------------------------------------------------------------------
bool S3Provider::uploadObject(const std::string& bucket, const std::string& key, const std::string& filePath) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::ifstream file(filePath, std::ios::binary);
    if (!file) return false;
    const auto startPos = file.tellg();
    file.seekg(0, std::ios::end);
    const curl_off_t fileSize = file.tellg();
    file.seekg(startPos);

    const std::string url = apiKey_->endpoint + "/" + bucket + "/" + key;

    std::ostringstream bodyHashStream;
    bodyHashStream << file.rdbuf();
    const std::string payloadHash = sha256Hex(bodyHashStream.str());
    file.clear();
    file.seekg(startPos);

    std::map<std::string, std::string> hdrMap{{"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
                                              {"x-amz-content-sha256", payloadHash},
                                              {"x-amz-date", util::getCurrentTimestamp()}};

    const std::string authHeader = buildAuthorizationHeader("PUT", "/" + bucket + "/" + key, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Content-Type: application/octet-stream");
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    auto readFn = +[](char* buf, size_t sz, size_t nm, void* ud) -> size_t {
        auto* fin = static_cast<std::ifstream*>(ud);
        fin->read(buf, static_cast<std::streamsize>(sz * nm));
        return static_cast<size_t>(fin->gcount());
    };

    curl_easy_setopt(curl, CURLOPT_READFUNCTION, readFn);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(curl, CURLOPT_READDATA, &file);
    curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, fileSize);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

bool S3Provider::downloadObject(const std::string& bucket, const std::string& key, const std::string& outputPath) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::ofstream file(outputPath, std::ios::binary);
    if (!file) {
        curl_easy_cleanup(curl);
        return false;
    }

    const std::string uriPath = "/" + bucket + "/" + key;
    const std::string url = apiKey_->endpoint + uriPath;

    // For GET we follow AWS guideline: payload hash = "UNSIGNED-PAYLOAD" and do NOT send x-amz-content-sha256 header
    const std::string payloadHash = "UNSIGNED-PAYLOAD";
    std::map<std::string, std::string> hdrMap{{"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
                                              {"x-amz-content-sha256", payloadHash},
                                              {"x-amz-date", util::getCurrentTimestamp()}};
    const std::string authHeader = buildAuthorizationHeader("GET", uriPath, hdrMap, payloadHash);

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
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFn);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    file.close();
    return res == CURLE_OK;
}

bool S3Provider::deleteObject(const std::string& bucket, const std::string& key) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    const std::string url = apiKey_->endpoint + "/" + bucket + "/" + key;
    const std::string payloadHash = sha256Hex("");

    std::map<std::string, std::string> hdrMap{{"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
                                              {"x-amz-content-sha256", payloadHash},
                                              {"x-amz-date", util::getCurrentTimestamp()}};

    const std::string authHeader = buildAuthorizationHeader("DELETE", "/" + bucket + "/" + key, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

std::string S3Provider::sha256Hex(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data.c_str(), data.size(), hash);
    std::ostringstream oss;
    for (unsigned char c : hash) oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    return oss.str();
}

std::string S3Provider::hmacSha256Hex(const std::string& key, const std::string& data) {
    unsigned char* digest;
    digest = HMAC(EVP_sha256(), key.c_str(), key.length(), (unsigned char*)data.c_str(), data.length(), NULL, NULL);
    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i
         ++)
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return oss.str();
}

std::string S3Provider::buildAuthorizationHeader(const std::string& method,
                                                 const std::string& fullPath, // may include ?query
                                                 const std::map<std::string, std::string>& headers,
                                                 const std::string& payloadHash) {
    // Split path and query for canonical request
    std::string canonicalPath = fullPath;
    std::string canonicalQuery;
    auto qpos = fullPath.find('?');
    if (qpos != std::string::npos) {
        canonicalPath = fullPath.substr(0, qpos);
        canonicalQuery = fullPath.substr(qpos + 1);
        if (canonicalQuery.find('=') == std::string::npos) canonicalQuery += '='; // ensure "uploads=" not "uploads"
    }
    // Per AWS spec, query parameters must be URI‑encoded and sorted; we only ever send fixed params so safe.

    const std::string service = "s3";
    const std::string algorithm = "AWS4-HMAC-SHA256";
    const std::string amzDate = headers.at("x-amz-date");
    const std::string dateStamp = util::getDate();

    // 1. Canonical headers
    std::string canonicalHeaders;
    std::string signedHeaders;
    for (auto it = headers.begin(); it != headers.end(); ++it) {
        canonicalHeaders += it->first + ":" + it->second + "\n";
        signedHeaders += it->first;
        if (std::next(it) != headers.end()) signedHeaders += ";";
    }

    // 2. Canonical request
    std::ostringstream canonicalRequestStream;
    canonicalRequestStream << method << "\n"
        << canonicalPath << "\n"
        << canonicalQuery << "\n"
        << canonicalHeaders << "\n"
        << signedHeaders << "\n"
        << payloadHash;

    const std::string canonicalRequest = canonicalRequestStream.str();
    const std::string hashedCanonicalRequest = sha256Hex(canonicalRequest);

    // 3. String to sign
    const std::string credentialScope = dateStamp + "/" + apiKey_->region + "/" + service + "/aws4_request";
    std::ostringstream stringToSignStream;
    stringToSignStream << algorithm << "\n" << amzDate << "\n" << credentialScope << "\n" << hashedCanonicalRequest;
    const std::string stringToSign = stringToSignStream.str();

    // 4. Calculate signature key
    auto hmac = [](const std::string& key, const std::string& data) {
        unsigned char* digest =
            HMAC(EVP_sha256(), key.data(), key.size(), reinterpret_cast<const unsigned char*>(data.data()), data.size(),
                 nullptr, nullptr);
        return std::string(reinterpret_cast<char*>(digest), SHA256_DIGEST_LENGTH);
    };

    std::string kDate = hmac("AWS4" + apiKey_->secret_access_key, dateStamp);
    std::string kRegion = hmac(kDate, apiKey_->region);
    std::string kService = hmac(kRegion, service);
    std::string kSigning = hmac(kService, "aws4_request");

    unsigned char sigRaw[SHA256_DIGEST_LENGTH];
    HMAC(EVP_sha256(), kSigning.data(), kSigning.size(), reinterpret_cast<const unsigned char*>(stringToSign.data()),
         stringToSign.size(), sigRaw, nullptr);

    std::ostringstream sigHex;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++
         i)
        sigHex << std::hex << std::setw(2) << std::setfill('0') << (int)sigRaw[i];

    // 5. Final header
    std::ostringstream authHeader;
    authHeader << algorithm << " " << "Credential=" << apiKey_->access_key << "/" << credentialScope << ", "
        << "SignedHeaders=" << signedHeaders << ", " << "Signature=" << sigHex.str();

    return authHeader.str();
}

// -------------------------------------------------------------------------
// Multipart helpers (initiate/uploadPart/complete/abort)
// Header lifetimes fixed + POSTFIELDSIZE_LARGE set
// -------------------------------------------------------------------------
std::string S3Provider::initiateMultipartUpload(const std::string& bucket, const std::string& key) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";

    const std::string uri = "/" + bucket + "/" + key + "?uploads";
    const std::string url = apiKey_->endpoint + uri;

    const std::string payloadHash = "UNSIGNED-PAYLOAD";
    std::map<std::string, std::string> hdrMap{{"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
                                              {"x-amz-content-sha256", payloadHash},
                                              {"x-amz-date", util::getCurrentTimestamp()}};
    const std::string authHeader = buildAuthorizationHeader("POST", uri, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    headers.add("x-amz-content-sha256: " + payloadHash);
    for (const auto& [k, v] : hdrMap) if (k != "x-amz-content-sha256") headers.add(k + ": " + v);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)0);
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION, +[](char* p, size_t s, size_t n, std::string* d) {
        d->append(p, s * n);
        return s * n;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode res = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        std::cerr << "initiateMultipartUpload failed: CURL=" << res << " HTTP=" << httpCode << "Resp: " << response
            << std::endl;
        return "";
    }

    std::smatch m;
    std::regex re("<UploadId>([^<]+)</UploadId>");
    if (std::regex_search(response, m, re) && m.size() > 1) return m[1].str();
    std::cerr << "Could not parse UploadId from response: " << response << std::endl;
    return "";
}

bool S3Provider::uploadPart(const std::string& bucket, const std::string& key, const std::string& uploadId,
                            int partNumber, const std::string& partData, std::string& etagOut) {
    std::cout << "Uploading part " << partNumber << " for " << key << " to bucket " << bucket << std::endl;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::ostringstream uriStream;
    uriStream << "/" << bucket << "/" << key << "?partNumber=" << partNumber << "&uploadId=" << uploadId;
    const std::string uri = uriStream.str();
    const std::string url = apiKey_->endpoint + uri;

    const std::string payloadHash = sha256Hex(partData);
    std::map<std::string, std::string> hdrMap{{"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
                                              {"x-amz-content-sha256", payloadHash},
                                              {"x-amz-date", util::getCurrentTimestamp()}};
    const std::string authHeader = buildAuthorizationHeader("PUT", uri, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Content-Type: application/octet-stream");
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    std::string respHdr;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, partData.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)partData.size());
    curl_easy_setopt(
        curl, CURLOPT_HEADERFUNCTION, +[](char* b, size_t s, size_t n, std::string* out) {
        out->append(b, s * n);
        return s * n;
        });
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &respHdr);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) return false;

    auto pos = respHdr.find("ETag:");
    if (pos == std::string::npos) return false;
    pos += 5;                                                                           // after "ETag:"
    while (pos < respHdr.size() && (respHdr[pos] == ' ' || respHdr[pos] == '	')) ++pos; // skip spaces
    auto end = respHdr.find_first_of("\r\n", pos);
    etagOut = respHdr.substr(pos, end - pos);
    // trim trailing spaces
    while (!etagOut.empty() && (etagOut.back() == ' ' || etagOut.back() == '	')) etagOut.pop_back();
    if (etagOut.empty()) return false;
    return true;
}

bool S3Provider::completeMultipartUpload(const std::string& bucket, const std::string& key, const std::string& uploadId,
                                         const std::vector<std::string>& etags) {
    CURL* curl = curl_easy_init();
    if (!curl || etags.empty()) return false;

    std::ostringstream uriStream;
    uriStream << "/" << bucket << "/" << key << "?uploadId=" << uploadId;
    const std::string uri = uriStream.str();
    const std::string url = apiKey_->endpoint + uri;

    std::ostringstream xml;
    xml << "<CompleteMultipartUpload>";
    for (size_t i = 0; i < etags.size(); ++i)
        xml << "<Part><PartNumber>" << (i + 1) << "</PartNumber><ETag>" << etags[
            i] << "</ETag></Part>";
    xml << "</CompleteMultipartUpload>";
    const std::string body = xml.str();

    const std::string payloadHash = sha256Hex(body);
    std::map<std::string, std::string> hdrMap{{"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
                                              {"x-amz-content-sha256", payloadHash},
                                              {"x-amz-date", util::getCurrentTimestamp()}};
    const std::string authHeader = buildAuthorizationHeader("POST", uri, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Content-Type: application/xml");
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE, (curl_off_t)body.size());
    curl_easy_setopt(
        curl, CURLOPT_WRITEFUNCTION, +[](char* p, size_t s, size_t n, std::string* d) {
        d->append(p, s * n);
        return s * n;
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    const CURLcode res = curl_easy_perform(curl);

    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || httpCode != 200) {
        // optional: log response for debugging
        return false;
    }
    return true;
    return res == CURLE_OK;
}

bool S3Provider::abortMultipartUpload(const std::string& bucket, const std::string& key, const std::string& uploadId) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    std::ostringstream uriStream;
    uriStream << "/" << bucket << "/" << key << "?uploadId=" << uploadId;
    const std::string uri = uriStream.str();
    const std::string url = apiKey_->endpoint + uri;

    const std::string payloadHash = sha256Hex("");
    std::map<std::string, std::string> hdrMap{{"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
                                              {"x-amz-content-sha256", payloadHash},
                                              {"x-amz-date", util::getCurrentTimestamp()}};
    const std::string authHeader = buildAuthorizationHeader("DELETE", uri, hdrMap, payloadHash);

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
bool S3Provider::uploadLargeObject(const std::string& bucket, const std::string& key, const std::string& filePath,
                                   size_t partSize) {
    std::ifstream file(filePath, std::ios::binary);
    if (!file) return false;

    const std::string uploadId = initiateMultipartUpload(bucket, key);
    if (uploadId.empty()) return false;

    std::vector<std::string> etags;
    int partNo = 1;
    bool ok = true;

    while (true) {
        std::string part(partSize, ' ');
        file.read(&part[0], partSize);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) break; // EOF reached
        part.resize(bytesRead);

        std::string etag;
        ok = uploadPart(bucket, key, uploadId, partNo, part, etag);
        if (!ok) break;
        etags.push_back(etag);
        ++partNo;
    }

    if (!ok || etags.empty()) {
        abortMultipartUpload(bucket, key, uploadId);
        return false;
    }

    return completeMultipartUpload(bucket, key, uploadId, etags);
}

std::vector<std::string> S3Provider::listObjects(const std::string& bucket, const std::string& prefix) {
    std::vector<std::string> allKeys;
    std::string continuationToken;
    bool moreResults = true;

    while (moreResults) {
        CURL* curl = curl_easy_init();
        if (!curl) break;

        std::ostringstream uri;
        uri << "/" << bucket << "?list-type=2";
        if (!prefix.empty()) uri << "&prefix=" << curl_easy_escape(curl, prefix.c_str(), prefix.length());
        if (!continuationToken.empty()) {
            uri << "&continuation-token=" << curl_easy_escape(curl, continuationToken.c_str(), continuationToken.length());
        }

        const std::string uriStr = uri.str();
        const std::string url = apiKey_->endpoint + uriStr;

        const std::string payloadHash = "UNSIGNED-PAYLOAD";
        std::map<std::string, std::string> hdrMap{
            {"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
            {"x-amz-content-sha256", payloadHash},
            {"x-amz-date", util::getCurrentTimestamp()}
        };
        std::string authHeader = buildAuthorizationHeader("GET", uriStr, hdrMap, payloadHash);

        HeaderList headers;
        headers.add("Authorization: " + authHeader);
        for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* p, const size_t s, const size_t n, std::string* d) {
            d->append(p, s * n);
            return s * n;
        });
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        const CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) break;

        // Extract keys
        std::regex keyRe("<Key>([^<]+)</Key>");
        for (auto it = std::sregex_iterator(response.begin(), response.end(), keyRe); it != std::sregex_iterator(); ++it) {
            allKeys.push_back((*it)[1].str());
        }

        // Check for <IsTruncated>true</IsTruncated>
        moreResults = std::regex_search(response, std::regex("<IsTruncated>true</IsTruncated>"));

        // Extract <NextContinuationToken>
        std::smatch tokenMatch;
        if (moreResults && std::regex_search(response, tokenMatch, std::regex("<NextContinuationToken>([^<]+)</NextContinuationToken>"))) {
            continuationToken = tokenMatch[1].str();
        } else {
            moreResults = false;
        }
    }

    return allKeys;
}

bool S3Provider::downloadToBuffer(const std::string& bucket, const std::string& key, std::string& outBuffer) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    const std::string uri = "/" + bucket + "/" + key;
    const std::string url = apiKey_->endpoint + uri;

    const std::string payloadHash = "UNSIGNED-PAYLOAD";
    std::map<std::string, std::string> hdrMap{
            {"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
            {"x-amz-content-sha256", payloadHash},
            {"x-amz-date", util::getCurrentTimestamp()}
    };
    const std::string authHeader = buildAuthorizationHeader("GET", uri, hdrMap, payloadHash);

    HeaderList headers;
    headers.add("Authorization: " + authHeader);
    for (const auto& [k, v] : hdrMap) headers.add(k + ": " + v);

    outBuffer.clear();
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.list);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, +[](char* ptr, size_t size, size_t nmemb, std::string* data) {
        data->append(ptr, size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outBuffer);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}

#include "storage/s3/S3Controller.hpp"
#include "types/vault/APIKey.hpp"
#include "util/timestamp.hpp"
#include "util/s3Helpers.hpp"
#include "logging/LogRegistry.hpp"

using namespace vh::cloud;
using namespace vh::types;
using namespace vh::util;
using namespace vh::logging;

std::map<std::string, std::string> S3Controller::buildHeaderMap(const std::string& payloadHash) const {
    return {
                {"host", apiKey_->endpoint.substr(apiKey_->endpoint.find("//") + 2)},
                {"x-amz-content-sha256", payloadHash},
                {"x-amz-date", getCurrentTimestamp()}
    };
}

SList S3Controller::makeSigHeaders(const std::string& method,
                                 const std::string& canonical,
                                 const std::string& payloadHash,
                                 const std::string& query) const {
    auto base = buildHeaderMap(payloadHash);      // host + dates
    const auto auth =  buildAuthorizationHeader(apiKey_, method, canonical, base, payloadHash, query);

    SList out;
    out.add("Authorization: " + auth);
    for (auto& [k, v] : base) out.add(k + ": " + v);
    return out;  // RAII slist
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

void S3Controller::setObjectContentHash(const std::filesystem::path& key, const std::string& hash) const {
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

    if (!resp.ok()) throw std::runtime_error(
        fmt::format("Failed to set content hash metadata on S3 object (HTTP {}): {}", resp.http, resp.body));
}

void S3Controller::setObjectEncryptionMetadata(const std::string& key, const std::string& iv_b64, unsigned int key_version) const {
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
    headers.add("x-amz-meta-vh-key-version: " + std::to_string(key_version));

    HttpResponse resp = performCurl([&](CURL* h) {
        curl_easy_setopt(h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers.get());
    });

    if (!resp.ok())
        LogRegistry::cloud()->error("[S3Provider] setObjectEncryptionMetadata failed for {}: CURL={} HTTP={}",
                                        key, resp.curl, resp.http);

    if (!resp.ok()) throw std::runtime_error(
        fmt::format("Failed to set encryption metadata on S3 object (HTTP {}): {}", resp.http, resp.body));
}
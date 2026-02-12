#include "storage/cloud/s3/S3Controller.hpp"
#include "types/vault/APIKey.hpp"
#include "util/s3Helpers.hpp"
#include "logging/LogRegistry.hpp"

#include <ctime>
#include <curl/curl.h>
#include <sstream>
#include <utility>

using namespace vh::cloud;
using namespace vh::types;
using namespace vh::util;
using namespace vh::logging;

S3Controller::S3Controller(const std::shared_ptr<api::APIKey>& apiKey, std::string bucket)
: apiKey_(apiKey), bucket_(std::move(bucket)) {
    if (!apiKey_) throw std::runtime_error("S3Provider requires a valid S3APIKey");
    ensureCurlGlobalInit();
}

S3Controller::~S3Controller() = default;

void S3Controller::deleteObject(const fs::path& key) const {
    const CurlEasy tmpHandle;
    const auto [canonical, url] = constructPaths(static_cast<CURL*>(tmpHandle), key);

    const std::string payloadHash = sha256Hex("");
    const SList hdrs = makeSigHeaders("DELETE", canonical, payloadHash);

    HttpResponse resp = performCurl([&](CURL* h){
        curl_easy_setopt(h, CURLOPT_URL, url.c_str());
        curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, hdrs.get());
    });

    if (!resp.ok()) LogRegistry::cloud()->error(
        "[S3Provider] deleteObject failed: CURL={} HTTP={} Response:\n{}",
        resp.curl, resp.http, resp.body
    );

    if (!resp.ok()) throw std::runtime_error(
        fmt::format("Failed to delete object from S3 (HTTP {}): {}", resp.http, resp.body));
}

std::u8string S3Controller::listObjects(const fs::path& prefix) const {
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

std::pair<std::string, std::string> S3Controller::constructPaths(CURL* curl, const fs::path& p, const std::string& query) const {
    const auto escapedKey = escapeKeyPreserveSlashes(curl, p);
    const auto canonicalPath = "/" + bucket_ + "/" + escapedKey + query;
    const auto url = apiKey_->endpoint + canonicalPath;
    return {canonicalPath, url};
}

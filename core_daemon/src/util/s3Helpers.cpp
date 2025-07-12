#include "util/s3Helpers.hpp"
#include "shared_util/u8.hpp"

#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <sstream>
#include <iomanip>
#include <regex>

namespace vh::util {

std::string sha256Hex(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(data.data()), data.size(), hash);
    std::ostringstream oss;
    for (unsigned char c : hash) oss << std::hex << std::setw(2) << std::setfill('0') << (int)c;
    return oss.str();
}

std::string hmacSha256Hex(const std::string& key, const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest, nullptr);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++
         i) oss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    return oss.str();
}

std::string hmacSha256Raw(const std::string& key, const std::string& data) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    HMAC(EVP_sha256(), key.data(), key.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), digest, nullptr);
    return std::string(reinterpret_cast<char*>(digest), SHA256_DIGEST_LENGTH);
}

std::string hmacSha256HexFromRaw(const std::string& rawKey, const std::string& data) {
    unsigned char sig[SHA256_DIGEST_LENGTH];
    HMAC(EVP_sha256(), rawKey.data(), rawKey.size(),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(), sig, nullptr);

    std::ostringstream oss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) oss << std::hex << std::setw(2) << std::setfill('0') << (int)sig[i];
    return oss.str();
}

std::string escapeKeyPreserveSlashes(CURL* curl, const std::filesystem::path& p) {
    std::ostringstream out;
    bool first = true;
    for (const auto& part : p) {
        if (!first) out << '/';
        first = false;

        std::string seg = to_utf8_string(part.u8string());
        char* esc = curl_easy_escape(curl, seg.c_str(), seg.length());
        if (!esc) {
            curl_easy_cleanup(curl);
            throw std::runtime_error("escape failed");
        }
        out << esc;
        curl_free(esc);
    }
    return out.str();
}

std::string composeMultiPartUploadXMLBody(const std::vector<std::string>& etags) {
    std::ostringstream xml;

    xml << "<CompleteMultipartUpload>";

    for (size_t i = 0; i < etags.size(); ++i)
        xml << "<Part><PartNumber>" << (i + 1) << "</PartNumber><ETag>" << etags[i] << "</ETag></Part>";

    xml << "</CompleteMultipartUpload>";

    return xml.str();
}

size_t writeToString(const char* ptr, const size_t size, const size_t nmemb, void* userdata) {
    auto* out = static_cast<std::string*>(userdata);
    out->append(ptr, size * nmemb);
    return size * nmemb;
}

void parsePagination(const std::string& response, std::string& continuationToken, bool& moreResults) {
    moreResults = std::regex_search(response, std::regex("<IsTruncated>true</IsTruncated>"));
    std::smatch tokenMatch;
    if (moreResults && std::regex_search(response, tokenMatch,
                                         std::regex("<NextContinuationToken>([^<]+)</NextContinuationToken>")))
        continuationToken = tokenMatch[1].str();
    else moreResults = false;
}

bool extractETag(const std::string& respHdr, std::string& etagOut) {
    auto pos = respHdr.find("ETag:");
    if (pos == std::string::npos) return false;
    pos += 5; // Skip "ETag:"
    while (pos < respHdr.size() && (respHdr[pos] == ' ' || respHdr[pos] == '\t')) ++pos;
    auto end = respHdr.find_first_of("\r\n", pos);
    etagOut = respHdr.substr(pos, end - pos);
    while (!etagOut.empty() && (etagOut.back() == ' ' || etagOut.back() == '\t')) etagOut.pop_back();

    return !etagOut.empty();
}


}
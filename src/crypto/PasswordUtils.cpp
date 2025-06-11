#include "crypto/PasswordUtils.hpp"
#include "crypto/SHA1.hpp"

#include <sstream>
#include <algorithm>
#include <cctype>
#include <curl/curl.h>

namespace vh::auth {
    std::unordered_set<std::string> PasswordUtils::dictionaryWords_;
    std::unordered_set<std::string> PasswordUtils::commonWeakPasswords_;

    unsigned short PasswordUtils::passwordStrengthCheck(const std::string& password) {
        if (password.empty()) return 1;

        unsigned short score = 0;

        size_t len = password.size();
        if (len >= 8) score += 20;
        if (len >= 12) score += 10;
        if (len >= 16) score += 10;
        if (len >= 20) score += 10;

        bool hasUpper = std::any_of(password.begin(), password.end(), ::isupper);
        bool hasLower = std::any_of(password.begin(), password.end(), ::islower);
        bool hasDigit = std::any_of(password.begin(), password.end(), ::isdigit);
        bool hasSymbol = std::any_of(password.begin(), password.end(), [](unsigned char c) {
            return std::ispunct(c);
        });

        if (hasLower) score += 15;
        if (hasUpper) score += 15;
        if (hasDigit) score += 15;
        if (hasSymbol) score += 15;

        if (!hasUpper && !hasDigit && !hasSymbol) score /= 2;

        // Clamp
        if (score < 1) score = 1;
        if (score > 100) score = 100;

        return score;
    }

    bool PasswordUtils::containsDictionaryWord(const std::string& password) {
        if (dictionaryWords_.empty()) return false; // fallback if not loaded

        std::string lowerPw = password;
        std::transform(lowerPw.begin(), lowerPw.end(), lowerPw.begin(), ::tolower);

        return std::ranges::any_of(dictionaryWords_,
                                   [&](const auto& word) { return lowerPw.contains(word); });
    }

    bool PasswordUtils::isCommonWeakPassword(const std::string& password) {
        std::string lowerPw = password;
        std::transform(lowerPw.begin(), lowerPw.end(), lowerPw.begin(), ::tolower);
        return commonWeakPasswords_.count(lowerPw) > 0;
    }

    size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
        size_t newLength = size * nmemb;
        s->append((char*)contents, newLength);
        return newLength;
    }

    bool PasswordUtils::isPwnedPassword(const std::string& password) {
        std::string sha1Hex = SHA1Hex(password); // Needs to be uppercase hex
        std::string prefix = sha1Hex.substr(0, 5);
        std::string suffix = sha1Hex.substr(5);

        std::string url = "https://api.pwnedpasswords.com/range/" + prefix;

        CURL* curl = curl_easy_init();
        if (!curl) return false;

        auto response = downloadURL(url);

        std::istringstream stream(response);
        std::string line;
        while (std::getline(stream, line)) {
            size_t delimPos = line.find(':');
            if (delimPos != std::string::npos) {
                std::string hashSuffix = line.substr(0, delimPos);
                if (hashSuffix == suffix) return true;
            }
        }

        return false;
    }

    void PasswordUtils::loadCommonWeakPasswordsFromURLs(const std::vector<std::string>& urls) {
        commonWeakPasswords_.clear();

        for (const auto& url : urls) {
            std::string data = downloadURL(url);

            std::istringstream stream(data);
            std::string password;
            while (std::getline(stream, password)) {
                password.erase(0, password.find_first_not_of(" \t\r\n"));
                password.erase(password.find_last_not_of(" \t\r\n") + 1);
                if (!password.empty()) {
                    std::transform(password.begin(), password.end(), password.begin(), ::tolower);
                    commonWeakPasswords_.insert(password);
                }
            }
        }
    }

    void PasswordUtils::loadDictionaryFromURL(const std::string& url) {
        dictionaryWords_.clear();

        std::string data = downloadURL(url);

        std::istringstream stream(data);
        std::string word;
        while (std::getline(stream, word)) {
            word.erase(0, word.find_first_not_of(" \t\r\n"));
            word.erase(word.find_last_not_of(" \t\r\n") + 1);
            if (!word.empty() && word.length() >= 3) {
                std::transform(word.begin(), word.end(), word.begin(), ::tolower);
                dictionaryWords_.insert(word);
            }
        }
    }

    std::string PasswordUtils::SHA1Hex(const std::string& input) {
        SHA1 sha1;
        sha1.update(input);
        std::string digest = sha1.final();

        // Convert to uppercase for HIBP
        std::transform(digest.begin(), digest.end(), digest.begin(), ::toupper);
        return digest;
    }

    size_t PasswordUtils::curlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
        size_t newLength = size * nmemb;
        s->append((char*)contents, newLength);
        return newLength;
    }

    std::string PasswordUtils::downloadURL(const std::string& url) {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to init curl");

        std::string response;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Vaulthalla-Password-Loader");

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) throw std::runtime_error("Failed to download URL: " + url);

        return response;
    }

} // namespace vh::auth

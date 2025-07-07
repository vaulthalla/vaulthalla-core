#include "util/http.hpp"

#include <regex>

namespace vh::util {

std::string extractCookie(const boost::beast::http::request<boost::beast::http::string_body>& req,
                          const std::string& key) {
    if (const auto it = req.find(boost::beast::http::field::cookie); it != req.end()) {
        const std::string cookieHeader = it->value();
        const std::regex cookieRegex(key + "=([^;]+)");
        std::smatch match;
        if (std::regex_search(cookieHeader, match, cookieRegex)) return match[1];
    }
    return "";
}

std::unordered_map<std::string, std::string> parse_query_params(const std::string& target) {
    std::unordered_map<std::string, std::string> params;

    const auto pos = target.find('?');
    if (pos == std::string::npos) return params;

    const std::string query = target.substr(pos + 1);
    std::istringstream stream(query);
    std::string pair;

    while (std::getline(stream, pair, '&')) {
        const auto eq = pair.find('=');
        if (eq != std::string::npos) {
            auto key = pair.substr(0, eq);
            const auto value = pair.substr(eq + 1);
            params[key] = value;
        }
    }

    return params;
}

}
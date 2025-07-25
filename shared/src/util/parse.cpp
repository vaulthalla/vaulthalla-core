#include "../../../shared/include/util/parse.hpp"

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

std::string url_decode(const std::string& value) {
    std::ostringstream result;
    for (size_t i = 0; i < value.length(); ++i) {
        if (value[i] == '%' && i + 2 < value.length()) {
            if (int hex = 0; std::istringstream(value.substr(i + 1, 2)) >> std::hex >> hex) {
                result << static_cast<char>(hex);
                i += 2;
            } else throw std::runtime_error("Invalid percent-encoding in URL");
        }
        else if (value[i] == '+') result << ' ';
        else result << value[i];
    }
    return result.str();
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
            const auto key = url_decode(pair.substr(0, eq));
            const auto value = url_decode(pair.substr(eq + 1));
            params[key] = value;
        }
    }

    return params;
}

}

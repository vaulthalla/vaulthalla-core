#include "util/parse.hpp"

#include <string>
#include <string_view>

namespace vh::util {

static void trim(std::string_view& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
}

std::string extractCookie(const boost::beast::http::request<boost::beast::http::string_body>& req,
                          const std::string& key) {
    const auto it = req.find(boost::beast::http::field::cookie);
    if (it == req.end()) return "";

    std::string_view cookies = it->value(); // beast::string_view compatible
    while (!cookies.empty()) {
        // Split next "name=value" chunk by ';'
        auto semi = cookies.find(';');
        std::string_view part = cookies.substr(0, semi);
        if (semi == std::string_view::npos) cookies = {};
        else cookies.remove_prefix(semi + 1);

        trim(part);
        if (part.empty()) continue;

        // Split name and value by '='
        auto eq = part.find('=');
        if (eq == std::string_view::npos) continue;

        std::string_view name = part.substr(0, eq);
        std::string_view value = part.substr(eq + 1);
        trim(name);
        trim(value);

        if (name == key) {
            // Strip optional quotes
            if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
                value = value.substr(1, value.size() - 2);

            return std::string(value);
        }
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

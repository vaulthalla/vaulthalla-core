#pragma once

#include <string>
#include <unordered_map>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>

namespace vh::util {

std::string extractCookie(const boost::beast::http::request<boost::beast::http::string_body>& req,
                      const std::string& key);

std::unordered_map<std::string, std::string> parse_query_params(const std::string& target);

std::string url_decode(const std::string& value);

}

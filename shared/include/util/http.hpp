#pragma once

#include <string>
#include <boost/beast/http.hpp>

namespace vh::util {

std::string extractCookie(const boost::beast::http::request<boost::beast::http::string_body>& req,
                          const std::string& key);

std::unordered_map<std::string, std::string> parse_query_params(const std::string& target);

}
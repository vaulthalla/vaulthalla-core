#pragma once

#include <boost/beast/http.hpp>

namespace vh::http {

namespace http = boost::beast::http;

using PreviewResponse = std::variant<
    http::response<http::vector_body<uint8_t>>,
    http::response<http::file_body>,
    http::response<http::string_body>
>;

}

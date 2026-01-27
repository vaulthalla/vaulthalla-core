#pragma once

#include "protocols/http/PreviewResponse.hpp"

#include <boost/beast/http.hpp>
#include <memory>

namespace vh::http {

namespace http = boost::beast::http;

struct PreviewRequest;

struct ImagePreviewHandler {
    static PreviewResponse handle(http::request<http::string_body>&& req, const std::unique_ptr<PreviewRequest>&& pr);
};

}

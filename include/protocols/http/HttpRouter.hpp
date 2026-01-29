#pragma once

#include "protocols/http/PreviewResponse.hpp"

#include <boost/beast/http.hpp>
#include <boost/beast/http/file_body.hpp>
#include <nlohmann/json_fwd.hpp>
#include <memory>
#include <string>

namespace vh::types {
struct File;
}

namespace vh::http {

class HttpSession;

namespace http = boost::beast::http;

struct HttpRouter {
    static PreviewResponse route(http::request<http::string_body>&& req);

    static PreviewResponse handlePreview(http::request<http::string_body>&& req);

    static PreviewResponse handleAuthSession(http::request<http::string_body>&& req);

    static PreviewResponse makeResponse(const http::request<http::string_body>& req,
                                        std::vector<uint8_t>&& data,
                                        const std::string& mime_type,
                                        bool cacheHit = false);

    static PreviewResponse makeResponse(const http::request<http::string_body>& req,
                                        http::file_body::value_type data,
                                        const std::string& mime_type,
                                        bool cacheHit = false);

    static PreviewResponse makeErrorResponse(const http::request<http::string_body>& req,
                                             const std::string& msg,
                                             const http::status& status = http::status::not_found);

    static PreviewResponse makeJsonResponse(const http::request<http::string_body>& req,
                                            const nlohmann::json& j);

    static std::string authenticateRequest(const http::request<http::string_body>& req);
};

}
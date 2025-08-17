#pragma once

#include "protocols/http/PreviewResponse.hpp"

#include <boost/beast/http.hpp>
#include <memory>
#include <filesystem>

namespace vh::auth {
class AuthManager;
}

namespace vh::storage {
class StorageManager;
class StorageEngine;
}

namespace vh::types {
struct File;
}

namespace vh::http {

class HttpSession;
class ImagePreviewHandler;
class PdfPreviewHandler;

namespace http = boost::beast::http;

class HttpRouter {
public:
    HttpRouter();

    PreviewResponse route(http::request<http::string_body>&& req) const;

    static http::response<http::string_body> makeErrorResponse(const http::request<http::string_body>& req,
                                                               const std::string& msg);

private:
    std::shared_ptr<auth::AuthManager> authManager_;
    std::shared_ptr<storage::StorageManager> storageManager_;

    std::shared_ptr<ImagePreviewHandler> imagePreviewHandler_;
    std::shared_ptr<PdfPreviewHandler> pdfPreviewHandler_;

    PreviewResponse handleCachedPreview(const std::shared_ptr<storage::StorageEngine>& engine,
                             const std::shared_ptr<types::File>& file,
                             const http::request<http::string_body>& req, unsigned int size) const;
};

}
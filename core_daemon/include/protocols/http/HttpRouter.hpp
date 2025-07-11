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
}

namespace vh::http {

class HttpSession;
class ImagePreviewHandler;
class PdfPreviewHandler;

namespace http = boost::beast::http;

class HttpRouter {
public:
    explicit HttpRouter(const std::shared_ptr<auth::AuthManager>& authManager,
                        const std::shared_ptr<storage::StorageManager>& storageManager);

    PreviewResponse route(http::request<http::string_body>&& req) const;

    static http::response<http::string_body> makeErrorResponse(const http::request<http::string_body>& req,
                                                               const std::string& msg);

private:
    std::shared_ptr<auth::AuthManager> authManager_;
    std::shared_ptr<storage::StorageManager> storageManager_;

    std::shared_ptr<ImagePreviewHandler> imagePreviewHandler_;
    std::shared_ptr<PdfPreviewHandler> pdfPreviewHandler_;

    PreviewResponse handleCachedPreview(unsigned int vaultId, const std::filesystem::path& rel_path,
                             const std::string& mime_type, const http::request<http::string_body>& req,
                             unsigned int size) const;
};

}
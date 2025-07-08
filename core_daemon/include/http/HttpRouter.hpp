#pragma once

#include "http/PreviewResponse.hpp"

#include <boost/beast/http.hpp>
#include <memory>

namespace vh::auth { class AuthManager; }
namespace vh::storage { class StorageManager; }

namespace vh::http {

class HttpSession;
class ImagePreviewHandler;

namespace http = boost::beast::http;

class HttpRouter {
public:
    explicit HttpRouter(const std::shared_ptr<auth::AuthManager>& authManager,
                        const std::shared_ptr<storage::StorageManager>& storageManager);

    PreviewResponse route(http::request<http::string_body>&& req) const;

private:
    std::shared_ptr<auth::AuthManager> authManager_;
    std::shared_ptr<storage::StorageManager> storageManager_;

    std::shared_ptr<ImagePreviewHandler> imagePreviewHandler_;
};

}

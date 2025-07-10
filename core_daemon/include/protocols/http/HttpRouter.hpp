#pragma once

#include "protocols/http/PreviewResponse.hpp"

#include <boost/beast/http.hpp>
#include <memory>

namespace vh::auth { class AuthManager; }
namespace vh::storage { class StorageManager; }

namespace vh::http {

class HttpSession;

namespace http = boost::beast::http;

class HttpRouter {
public:
    explicit HttpRouter(const std::shared_ptr<auth::AuthManager>& authManager,
                        const std::shared_ptr<storage::StorageManager>& storageManager);

    PreviewResponse route(http::request<http::string_body>&& req) const;

    static http::response<http::string_body> makeErrorResponse(const http::request<http::string_body>& req, const std::string& msg);

private:
    std::shared_ptr<auth::AuthManager> authManager_;
    std::shared_ptr<storage::StorageManager> storageManager_;
};

}

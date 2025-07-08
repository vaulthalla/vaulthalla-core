#include "protocols/http/HttpRouter.hpp"
#include "protocols/http/handlers/ImagePreviewHandler.hpp"
#include "util/parse.hpp"
#include "database/Queries/FileQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "auth/AuthManager.hpp"

namespace vh::http {

HttpRouter::HttpRouter(const std::shared_ptr<auth::AuthManager>& authManager,
                       const std::shared_ptr<storage::StorageManager>& storageManager)
    : authManager_(authManager), storageManager_(storageManager), imagePreviewHandler_(std::make_shared<ImagePreviewHandler>(storageManager)) {
    if (!authManager_) throw std::invalid_argument("AuthManager cannot be null");
    if (!storageManager) throw std::invalid_argument("StorageManager cannot be null");
}

PreviewResponse HttpRouter::route(http::request<http::string_body>&& req) const {
    if (req.method() != http::verb::get || !req.target().starts_with("/preview")) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::content_type, "text/plain");
        res.body() = "Invalid request";
        res.prepare_payload();
        return res;
    }

    if (!config::ConfigRegistry::get().advanced.dev_mode) {
        try {
            const auto refresh_token = util::extractCookie(req, "refresh");
            authManager_->validateRefreshToken(refresh_token);
        } catch (const std::exception& e) {
            http::response<http::string_body> res{http::status::unauthorized, req.version()};
            res.set(http::field::content_type, "text/plain");
            res.body() = "Unauthorized: " + std::string(e.what());
            res.prepare_payload();
            return res;
        }
    }

    auto params = util::parse_query_params(req.target());
    const auto vault_it = params.find("vault_id");
    const auto path_it = params.find("path");
    if (vault_it == params.end() || path_it == params.end()) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.body() = "Missing vault_id or path";
        res.prepare_payload();
        return res;
    }

    const auto vault_id = std::stoi(vault_it->second);
    const std::string rel_path = path_it->second;
    const std::string mime_type = database::FileQueries::getMimeType(vault_id, {rel_path});

    if (mime_type.starts_with("image/") || mime_type.ends_with("octet-stream"))
        return imagePreviewHandler_->handle(std::move(req), vault_id, rel_path, params);

    http::response<http::string_body> res{http::status::unsupported_media_type, req.version()};
    res.body() = "Unsupported preview type: " + mime_type;
    res.prepare_payload();
    return res;
}

}

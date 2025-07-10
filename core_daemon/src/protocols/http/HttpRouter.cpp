#include "protocols/http/HttpRouter.hpp"
#include "protocols/http/handlers/ImagePreviewHandler.hpp"
#include "protocols/http/handlers/PdfPreviewHandler.hpp"
#include "util/parse.hpp"
#include "storage/StorageManager.hpp"
#include "database/Queries/FileQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "auth/AuthManager.hpp"

#include <iostream>

namespace vh::http {

HttpRouter::HttpRouter(const std::shared_ptr<auth::AuthManager>& authManager,
                       const std::shared_ptr<storage::StorageManager>& storageManager)
    : authManager_(authManager), storageManager_(storageManager),
imagePreviewHandler_(std::make_shared<ImagePreviewHandler>(storageManager)),
      pdfPreviewHandler_(std::make_shared<PdfPreviewHandler>(storageManager)) {
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
    std::string rel_path = path_it->second;
    const std::string mime_type = database::FileQueries::getMimeType(vault_id, {rel_path});

    const auto vault = storageManager_->getVault(vault_id);
    if (vault->type == types::VaultType::S3) {
        const auto vaultRoot = storageManager_->getCloudEngine(vault_id)->root_directory();
        if (rel_path.starts_with("/")) rel_path.erase(0, 1); // Remove leading slash if present
        const auto pathToJpeg = vaultRoot / rel_path;

        // Force `.jpg` extension if this is a PDF or image preview
        if (mime_type.starts_with("image/") || mime_type.starts_with("application/")) {
            std::filesystem::path jpegPath = pathToJpeg;
            jpegPath.replace_extension(".jpg");

            if (std::filesystem::exists(jpegPath)) {
                http::file_body::value_type body;
                boost::beast::error_code ec;
                body.open(jpegPath.c_str(), boost::beast::file_mode::scan, ec);
                if (ec) {
                    std::cerr << "[PdfPreviewHandler] Error opening JPEG file: " << ec.message() << std::endl;
                    return makeErrorResponse(req, "Failed to open preview file");
                }

                http::response<http::file_body> res{
                    std::piecewise_construct,
                    std::make_tuple(std::move(body)),
                    std::make_tuple(http::status::ok, req.version())
                };
                res.set(http::field::content_type, "image/jpeg");
                res.content_length(body.size());
                res.keep_alive(req.keep_alive());
                return res;
            } else {
                std::cerr << "[PdfPreviewHandler] Cached preview not found: " << jpegPath << std::endl;
                return makeErrorResponse(req, "Preview not found");
            }
        }
    }

    if (mime_type.starts_with("image/") || mime_type.ends_with("/octet-stream"))
        return imagePreviewHandler_->handle(std::move(req), vault_id, rel_path, params);

    if (mime_type.ends_with("/pdf")) return pdfPreviewHandler_->handle(std::move(req), vault_id, rel_path, params);

    http::response<http::string_body> res{http::status::unsupported_media_type, req.version()};
    res.body() = "Unsupported preview type: " + mime_type;
    res.prepare_payload();
    return res;
}

http::response<http::string_body> HttpRouter::makeErrorResponse(const http::request<http::string_body>& req, const std::string& msg) {
    http::response<http::string_body> res{http::status::not_found, req.version()};
    res.set(http::field::content_type, "text/plain");
    res.body() = msg;
    res.prepare_payload();
    return res;
}

}

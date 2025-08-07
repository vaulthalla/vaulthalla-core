#include "protocols/http/HttpRouter.hpp"
#include "util/parse.hpp"
#include "storage/StorageManager.hpp"
#include "storage/StorageEngine.hpp"
#include "database/Queries/FileQueries.hpp"
#include "config/ConfigRegistry.hpp"
#include "auth/AuthManager.hpp"
#include "protocols/http/handlers/ImagePreviewHandler.hpp"
#include "protocols/http/handlers/PdfPreviewHandler.hpp"
#include "util/files.hpp"
#include "services/ServiceDepsRegistry.hpp"
#include "logging/LogRegistry.hpp"

#include <ranges>

using namespace vh::services;
using namespace vh::logging;
using namespace vh::config;

namespace vh::http {

HttpRouter::HttpRouter()
    : authManager_(ServiceDepsRegistry::instance().authManager),
      storageManager_(ServiceDepsRegistry::instance().storageManager),
      imagePreviewHandler_(std::make_shared<ImagePreviewHandler>(storageManager_)),
      pdfPreviewHandler_(std::make_shared<PdfPreviewHandler>(storageManager_)) {
    if (!authManager_) throw std::invalid_argument("AuthManager cannot be null");
    if (!storageManager_) throw std::invalid_argument("StorageManager cannot be null");
}

PreviewResponse HttpRouter::route(http::request<http::string_body>&& req) const {
    if (req.method() != http::verb::get || !req.target().starts_with("/preview")) {
        http::response<http::string_body> res{http::status::bad_request, req.version()};
        res.set(http::field::content_type, "text/plain");
        res.body() = "Invalid request";
        res.prepare_payload();
        return res;
    }

    if (!ConfigRegistry::get().advanced.dev_mode) {
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
    std::filesystem::path rel_path = path_it->second;
    const std::string mime_type = database::FileQueries::getMimeType(vault_id, {rel_path});

    const auto size_it = params.find("size");

    if (size_it != params.end() && !size_it->second.empty()) {
        const auto thumbnail_sizes = config::ConfigRegistry::get().caching.thumbnails.sizes;
        const auto size = std::stoi(size_it->second);
        if (std::ranges::find(thumbnail_sizes.begin(), thumbnail_sizes.end(), size) != thumbnail_sizes.end()) {
            try {
                return handleCachedPreview(vault_id, rel_path, mime_type, req, size);
            } catch (const std::exception& e) {
                LogRegistry::http()->error("[HttpRouter] Error handling cached preview for {}: {}", rel_path.string(), e.what());
                http::response<http::string_body> res{http::status::internal_server_error, req.version()};
                res.set(http::field::content_type, "text/plain");
                res.body() = "Failed to handle cached preview: " + std::string(e.what());
                res.prepare_payload();
                return res;
            }
        }
    }

    const auto filename = rel_path.filename().string();
    if (mime_type.starts_with("image/" || filename.ends_with(".webp")))
        return imagePreviewHandler_->handle(std::move(req), vault_id, rel_path, params);

    if (mime_type.ends_with("/pdf")) return pdfPreviewHandler_->handle(std::move(req), vault_id, rel_path, params);

    http::response<http::string_body> res{http::status::unsupported_media_type, req.version()};
    res.body() = "Unsupported preview type: " + mime_type;
    res.prepare_payload();
    return res;
}

PreviewResponse HttpRouter::handleCachedPreview(const unsigned int vaultId, const std::filesystem::path& rel_path,
                                     const std::string& mime_type, const http::request<http::string_body>& req,
                                     const unsigned int size) const {
    const auto engine = storageManager_->getEngine(vaultId);
    const auto pathToJpegCache = engine->paths->absPath(rel_path, PathType::THUMBNAIL_ROOT) / std::to_string(size);

    // Force `.jpg` extension if this is a PDF or image preview
    if (mime_type.starts_with("image/") || mime_type.starts_with("application/")) {
        std::filesystem::path jpegPath = pathToJpegCache;
        if (jpegPath.extension() != ".jpg" && jpegPath.extension() != ".jpeg") jpegPath.append(".jpg");

        if (std::filesystem::exists(jpegPath)) {
            const auto data = util::readFileToVector(jpegPath);

            // Return decrypted image in memory
            http::response<http::string_body> res{
                http::status::ok, req.version()
            };
            res.set(http::field::content_type, "image/jpeg");
            res.body() = std::string(data.begin(), data.end());
            res.prepare_payload();
            res.keep_alive(req.keep_alive());
            return res;
        }

        throw std::runtime_error("[PdfPreviewHandler] JPEG file does not exist: " + jpegPath.string());
    }
    return makeErrorResponse(req, "Unsupported preview type: " + mime_type);
}

http::response<http::string_body> HttpRouter::makeErrorResponse(const http::request<http::string_body>& req,
                                                                const std::string& msg) {
    http::response<http::string_body> res{http::status::not_found, req.version()};
    res.set(http::field::content_type, "text/plain");
    res.body() = msg;
    res.prepare_payload();
    return res;
}

}
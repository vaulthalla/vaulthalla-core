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
#include "types/File.hpp"
#include "types/FSEntry.hpp"
#include "storage/FSCache.hpp"
#include "types/stats/CacheStats.hpp"

#include <ranges>

using namespace vh::services;
using namespace vh::logging;
using namespace vh::config;
using namespace vh::types;

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

    if (!ConfigRegistry::get().dev.enabled) {
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

    const auto size_it = params.find("size");

    const auto engine = storageManager_->getEngine(vault_id);
    if (!engine) throw std::runtime_error("No storage engine found for vault with ID: " + std::to_string(vault_id));

    const auto fusePath = engine->paths->absRelToAbsRel(rel_path, PathType::VAULT_ROOT, PathType::FUSE_ROOT);

    const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(fusePath);
    if (!entry) throw std::runtime_error("File not found in cache: " + fusePath.string());
    if (entry->isDirectory()) throw std::runtime_error("Cannot generate preview for directory: " + fusePath.string());
    const auto file = std::static_pointer_cast<File>(entry);

    if (!file->mime_type) {
        LogRegistry::http()->error("[HttpRouter] File {} has no MIME type", rel_path.string());
        http::response<http::string_body> res{http::status::unsupported_media_type, req.version()};
        res.set(http::field::content_type, "text/plain");
        res.body() = "File has no MIME type";
        res.prepare_payload();
        return res;
    }

    if (size_it != params.end() && !size_it->second.empty()) {
        const auto thumbnail_sizes = config::ConfigRegistry::get().caching.thumbnails.sizes;
        const auto size = std::stoi(size_it->second);
        if (std::ranges::find(thumbnail_sizes.begin(), thumbnail_sizes.end(), size) != thumbnail_sizes.end()) {
            const auto filename = std::to_string(size) + ".jpg";
            const auto pathToJpegCache = engine->paths->thumbnailRoot / file->base32_alias / filename;
            if (file->mime_type && (file->mime_type->starts_with("image/") || file->mime_type->starts_with("application/"))) {
                if (std::filesystem::exists(pathToJpegCache)) {
                    const auto data = util::readFileToVector(pathToJpegCache);

                    // Return decrypted image in memory
                    http::response<http::string_body> res{
                        http::status::ok, req.version()
                    };
                    res.set(http::field::content_type, "image/jpeg");
                    res.body() = std::string(data.begin(), data.end());
                    res.prepare_payload();
                    res.keep_alive(req.keep_alive());
                    ServiceDepsRegistry::instance().httpCacheStats->record_hit(data.size());
                    return res;
                }
            }
            ServiceDepsRegistry::instance().httpCacheStats->record_miss();
        }
    }

    if (file->mime_type->starts_with("image/") || file->mime_type->ends_with("/pdf")) {
        ScopedOpTimer timer(ServiceDepsRegistry::instance().httpCacheStats.get());
        PreviewResponse res;

        if (file->mime_type->starts_with("image/")) res = imagePreviewHandler_->handle(std::move(req), vault_id, rel_path, params);
        else res = pdfPreviewHandler_->handle(std::move(req), vault_id, rel_path, params);
        return res;
    }

    return makeErrorResponse(req, "Unsupported preview type: " + (file->mime_type ? *file->mime_type : "unknown"));
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
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
#include "types/File.hpp"
#include "types/FSEntry.hpp"
#include "storage/FSCache.hpp"
#include "types/stats/CacheStats.hpp"
#include "protocols/http/PreviewRequest.hpp"
#include "logging/LogRegistry.hpp"

#include <nlohmann/json.hpp>

using namespace vh::services;
using namespace vh::logging;
using namespace vh::config;
using namespace vh::types;

namespace vh::http {

static std::vector<uint8_t> tryCacheRead(const std::shared_ptr<File>& f, const std::filesystem::path& thumbnailRoot,
                                         const unsigned int size) {
    if (const auto thumbnail_sizes = ConfigRegistry::get().caching.thumbnails.sizes;
        std::ranges::find(thumbnail_sizes.begin(), thumbnail_sizes.end(), size) != thumbnail_sizes.end()) {

        if (const auto pathToJpegCache = thumbnailRoot / f->base32_alias / std::string(std::to_string(size) + ".jpg");
            f->mime_type && (f->mime_type->starts_with("image/") || f->mime_type->starts_with("application/"))
            && std::filesystem::exists(pathToJpegCache))
            return util::readFileToVector(pathToJpegCache);

        ServiceDepsRegistry::instance().httpCacheStats->record_miss();
    }
    return {};
}

PreviewResponse HttpRouter::route(http::request<http::string_body>&& req) {
    if (req.method() != http::verb::get)
        return makeErrorResponse(req, "Invalid request", http::status::bad_request);

    if (req.target().starts_with("/auth/session"))
        return handleAuthSession(std::move(req));

    if (req.target().starts_with("/preview"))
        return handlePreview(std::move(req));

    return makeErrorResponse(req, "Not found", http::status::not_found);
}

PreviewResponse HttpRouter::handleAuthSession(http::request<http::string_body>&& req) {
    if (ConfigRegistry::get().dev.enabled) return makeJsonResponse(req, nlohmann::json{{"ok", true}});

    try {
        const auto refresh = util::extractCookie(req, "refresh");
        if (refresh.empty()) {
            LogRegistry::http()->warn("[HttpRouter] Refresh token not set");
            return makeErrorResponse(req, "Refresh token not set", http::status::bad_request);
        }
        ServiceDepsRegistry::instance().authManager->validateRefreshToken(refresh);
        const auto session = ServiceDepsRegistry::instance().authManager->sessionManager()->getClientSession(refresh);
        if (!session || !session->getUser()) {
            LogRegistry::http()->warn("[HttpRouter]: Invalid refresh token. Unable to locate session.");
            return makeErrorResponse(req, "Not found", http::status::not_found);
        }

        // If validateRefreshToken only throws/returns void, extend it to return user/session info.
        // Optionally mint an access token here:
        // auto access = authManager->mintAccessToken(session.user_id);

        nlohmann::json j{
                {"ok", true},
                {"user_id", session->getUser()->id},
                // {"access_token", access.token},
                // {"expires_in", access.expires_in}
            };

        return makeJsonResponse(req, j);
    } catch (const std::exception& e) {
        LogRegistry::http()->warn("[HttpRouter]: Invalid refresh token: %s", e.what());
        return makeErrorResponse(req, std::string("Unauthorized: ") + e.what(), http::status::unauthorized);
    }
}

std::string HttpRouter::authenticateRequest(const http::request<http::string_body>& req) {
    if (ConfigRegistry::get().dev.enabled) return "";
    try {
        const auto refresh_token = util::extractCookie(req, "refresh");
        ServiceDepsRegistry::instance().authManager->validateRefreshToken(refresh_token);
        return "";
    } catch (const std::exception& e) {
        return e.what();
    }
}

PreviewResponse HttpRouter::handlePreview(http::request<http::string_body>&& req) {
    if (req.method() != http::verb::get || !req.target().starts_with("/preview"))
        return makeErrorResponse(
            req, "Invalid request", http::status::bad_request);

    if (const auto error = authenticateRequest(req); !error.empty())
        return makeErrorResponse(
            req, "Unauthorized: " + error, http::status::unauthorized);

    std::unique_ptr<PreviewRequest> pr;
    try { pr = std::make_unique<PreviewRequest>(util::parse_query_params(req.target())); } catch (const std::exception&
        e) { return makeErrorResponse(req, e.what(), http::status::bad_request); }

    pr->engine = ServiceDepsRegistry::instance().storageManager->getEngine(pr->vault_id);
    if (!pr->engine)
        return makeErrorResponse(
            req, "No storage engine found for vault with id " + std::to_string(pr->vault_id),
            http::status::bad_request);

    const auto fusePath = pr->engine->paths->absRelToAbsRel(pr->rel_path, PathType::VAULT_ROOT, PathType::FUSE_ROOT);
    const auto entry = ServiceDepsRegistry::instance().fsCache->getEntry(fusePath);
    if (!entry) return makeErrorResponse(req, "File not found in cache");
    if (entry->isDirectory())
        return makeErrorResponse(req, "Requested preview file is a directory", http::status::bad_request);
    pr->file = std::static_pointer_cast<File>(entry);

    if (!pr->file->mime_type)
        return makeErrorResponse(req, "File has no mime type.", http::status::unsupported_media_type);

    if (pr->size)
        if (auto data = tryCacheRead(pr->file, pr->engine->paths->thumbnailRoot, *pr->size); !data.empty())
            return makeResponse(req, std::move(data), "image/jpeg", true);

    if (pr->file->mime_type->starts_with("image/") || pr->file->mime_type->ends_with("/pdf")) {
        ScopedOpTimer timer(ServiceDepsRegistry::instance().httpCacheStats.get());
        return pr->file->mime_type->starts_with("image/")
                   ? ImagePreviewHandler::handle(std::move(req), std::move(pr))
                   : PdfPreviewHandler::handle(std::move(req), std::move(pr));
    }

    return makeErrorResponse(
        req, "Unsupported preview type: " + (pr->file->mime_type ? *pr->file->mime_type : "unknown"));
}

PreviewResponse HttpRouter::makeResponse(const http::request<http::string_body>& req, std::vector<uint8_t>&& data,
                                         const std::string& mime_type, const bool cacheHit) {
    const auto size = data.size();

    http::response<http::vector_body<uint8_t> > res{
        std::piecewise_construct,
        std::make_tuple(std::move(data)),
        std::make_tuple(http::status::ok, req.version())
    };

    res.set(http::field::content_type, mime_type);
    res.content_length(size);
    res.keep_alive(req.keep_alive());

    if (cacheHit) ServiceDepsRegistry::instance().httpCacheStats->record_hit(size);

    return res;
}

PreviewResponse HttpRouter::makeResponse(const http::request<http::string_body>& req, http::file_body::value_type data,
                                         const std::string& mime_type, const bool cacheHit) {
    const auto size = data.size();

    http::response<http::file_body> res{
        std::piecewise_construct,
        std::make_tuple(std::move(data)),
        std::make_tuple(http::status::ok, req.version())
    };

    res.set(http::field::content_type, mime_type);
    res.content_length(size);
    res.keep_alive(req.keep_alive());

    if (cacheHit) ServiceDepsRegistry::instance().httpCacheStats->record_hit(size);

    return res;
}

PreviewResponse HttpRouter::makeJsonResponse(const http::request<http::string_body>& req, const nlohmann::json& j) {
    http::response<http::string_body> res{http::status::ok, req.version()};
    res.set(http::field::content_type, "application/json");
    res.body() = j.dump();
    res.prepare_payload();
    res.keep_alive(req.keep_alive());
    return res;
}

PreviewResponse HttpRouter::makeErrorResponse(const http::request<http::string_body>& req,
                                              const std::string& msg,
                                              const http::status& status) {
    http::response<http::string_body> res{status, req.version()};
    res.set(http::field::content_type, "text/plain");
    res.body() = msg;
    res.prepare_payload();
    return res;
}

}

#include "protocols/http/handlers/ImagePreviewHandler.hpp"
#include "util/imageUtil.hpp"
#include "util/files.hpp"
#include "database/Queries/FileQueries.hpp"
#include "logging/LogRegistry.hpp"
#include "protocols/http/PreviewRequest.hpp"
#include "protocols/http/HttpRouter.hpp"

#include <boost/beast/http/file_body.hpp>

using namespace vh::storage;
using namespace vh::util;
using namespace vh::logging;
using namespace vh::database;

namespace vh::http {

PreviewResponse ImagePreviewHandler::handle(http::request<http::string_body>&& req, const std::unique_ptr<PreviewRequest>&& pr) {
    try {
        const auto tmpPath = decrypt_file_to_temp(pr->vault_id, pr->rel_path, pr->engine);

        if (pr->size || pr->scale) {
            std::vector<uint8_t> resized = resize_and_compress_image(tmpPath, pr->scaleStr(), pr->sizeStr());
            return HttpRouter::makeResponse(req, std::move(resized), "image/jpeg");
        }

        boost::beast::error_code ec;
        http::file_body::value_type body;
        body.open(tmpPath.c_str(), boost::beast::file_mode::scan, ec);
        if (ec) return HttpRouter::makeErrorResponse(req, "File not found.", http::status::not_found);

        return HttpRouter::makeResponse(req, std::move(body), "image/jpeg");
    } catch (const std::exception& e) {
        LogRegistry::http()->error("[ImagePreviewHandler] Error handling image preview for {}: {}", pr->rel_path.string(), e.what());
        return HttpRouter::makeErrorResponse(req, "Failed to load image: " + std::string(e.what()), http::status::unsupported_media_type);
    }
}

} // namespace vh::http

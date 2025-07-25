#include "protocols/http/handlers/ImagePreviewHandler.hpp"
#include "util/imageUtil.hpp"
#include "../../../../../shared/include/util/files.hpp"
#include "database/Queries/FileQueries.hpp"
#include "shared/include/engine/StorageManager.hpp"

#include <boost/beast/http/file_body.hpp>
#include <iostream>

using namespace vh::storage;

namespace vh::http {

PreviewResponse ImagePreviewHandler::handle(http::request<http::string_body>&& req, int vault_id, const std::string& rel_path,
                                                    const std::unordered_map<std::string, std::string>& params) const {
    try {
        const auto scale_it = params.find("scale");
        const auto size_it = params.find("size");

        const auto engine = storageManager_->getEngine(vault_id);
        const auto tmpPath = util::decrypt_file_to_temp(vault_id, rel_path, engine);

        std::string file_path = engine->getAbsolutePath(rel_path);
        std::string mime_type = database::FileQueries::getMimeType(vault_id, {rel_path});

        if (scale_it != params.end() || size_it != params.end()) {
            std::optional<std::string> scale, size;
            if (scale_it != params.end()) scale = scale_it->second;
            if (size_it != params.end()) size = size_it->second;

            std::vector<uint8_t> resized = util::resize_and_compress_image(tmpPath, scale, size);

            http::response<http::vector_body<uint8_t>> res{http::status::ok, req.version()};
            res.set(http::field::content_type, mime_type);
            res.body() = std::move(resized);
            res.content_length(res.body().size());
            res.keep_alive(req.keep_alive());
            res.prepare_payload();
            return res;
        }

        boost::beast::error_code ec;
        http::file_body::value_type body;
        body.open(tmpPath.c_str(), boost::beast::file_mode::scan, ec);
        if (ec) {
            http::response<http::string_body> res{http::status::not_found, req.version()};
            res.set(http::field::content_type, "text/plain");
            res.body() = "File not found";
            res.prepare_payload();
            return res;
        }

        http::response<http::file_body> res{
            std::piecewise_construct,
            std::make_tuple(std::move(body)),
            std::make_tuple(http::status::ok, req.version())
        };
        res.set(http::field::content_type, mime_type);
        res.content_length(body.size());
        res.keep_alive(req.keep_alive());
        return res;
    } catch (const std::exception& e) {
        std::cerr << "[ImagePreviewHandler] Resize failed: " << e.what() << std::endl;
        http::response<http::string_body> err{http::status::unsupported_media_type, req.version()};
        err.set(http::field::content_type, "text/plain");
        err.body() = "Failed to load image: " + std::string(e.what());
        err.prepare_payload();
        return err;
    }
}

} // namespace vh::http

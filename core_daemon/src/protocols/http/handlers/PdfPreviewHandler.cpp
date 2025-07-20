#include "protocols/http/handlers/PdfPreviewHandler.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageManager.hpp"
#include "util/imageUtil.hpp"
#include "util/files.hpp"

#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>
#include <iostream>

namespace vh::http {

PreviewResponse PdfPreviewHandler::handle(http::request<http::string_body>&& req,
                                          int vault_id,
                                          const std::string& rel_path,
                                          const std::unordered_map<std::string, std::string>& params) const {
    try {
        const auto scale_it = params.find("scale");
        const auto size_it = params.find("size");

        const auto engine = storageManager_->getLocalEngine(vault_id);
        const std::string tmpPath = util::decrypt_file_to_temp(vault_id, rel_path, engine);
        const std::string mime_type = "image/jpeg";

        std::ifstream in(tmpPath, std::ios::binary | std::ios::ate);
        if (!in) throw std::runtime_error("Failed to open PDF for reading");
        const std::streamsize size = in.tellg();
        in.seekg(0);
        std::string buffer(size, '\0');
        if (!in.read(buffer.data(), size)) throw std::runtime_error("Failed to read decrypted PDF");

        auto doc = poppler::document::load_from_raw_data(buffer.data(), buffer.size());
        if (!doc || doc->is_locked()) throw std::runtime_error("Failed to load or unlock PDF");

        auto page = doc->create_page(0);
        if (!page) throw std::runtime_error("Failed to load first page");

        const double width = page->page_rect().width();
        const double height = page->page_rect().height();

        double scale = 1.0;
        if (scale_it != params.end()) {
            scale = std::stof(scale_it->second);
        } else if (size_it != params.end()) {
            const int max_dim = std::stoi(size_it->second);
            const double ratio = std::min(max_dim / width, max_dim / height);
            scale = ratio;
        }

        poppler::page_renderer renderer;
        renderer.set_render_hint(poppler::page_renderer::antialiasing, true);
        renderer.set_render_hint(poppler::page_renderer::text_antialiasing, true);
        poppler::image img = renderer.render_page(page, scale * 72.0, scale * 72.0);

        if (!img.is_valid()) throw std::runtime_error("Poppler failed to render the page");

        const int w = img.width();
        const int h = img.height();
        const int row_stride = img.bytes_per_row();
        const char* raw = img.data();

        std::vector<uint8_t> rgb(w * h * 3);

        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int pixel_offset = y * row_stride + x * 4;

                rgb[(y * w + x) * 3 + 0] = raw[pixel_offset + 0]; // R
                rgb[(y * w + x) * 3 + 1] = raw[pixel_offset + 1]; // G
                rgb[(y * w + x) * 3 + 2] = raw[pixel_offset + 2]; // B
            }
        }

        std::vector<uint8_t> jpeg_buf;
        util::compress_to_jpeg(rgb.data(), w, h, jpeg_buf);

        http::response<http::vector_body<uint8_t>> res{http::status::ok, req.version()};
        res.set(http::field::content_type, mime_type);
        res.body() = std::move(jpeg_buf);
        res.content_length(res.body().size());
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        return res;

    } catch (const std::exception& e) {
        std::cerr << "[PdfPreviewHandler] Error: " << e.what() << std::endl;
        http::response<http::string_body> res{http::status::unsupported_media_type, req.version()};
        res.set(http::field::content_type, "text/plain");
        res.body() = "Failed to preview PDF: " + std::string(e.what());
        res.prepare_payload();
        return res;
    }
}

}

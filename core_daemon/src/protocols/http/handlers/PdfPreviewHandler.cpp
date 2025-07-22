#include "protocols/http/handlers/PdfPreviewHandler.hpp"
#include "database/Queries/FileQueries.hpp"
#include "storage/StorageManager.hpp"
#include "util/imageUtil.hpp"
#include "util/files.hpp"

#include <fpdfview.h>
#include <iostream>

namespace vh::http {

PreviewResponse PdfPreviewHandler::handle(http::request<http::string_body>&& req,
                                          int vault_id,
                                          const std::string& rel_path,
                                          const std::unordered_map<std::string, std::string>& params) const {
    try {
        const auto scale_it = params.find("scale");
        const auto size_it = params.find("size");

        const auto engine = storageManager_->getEngine(vault_id);

        std::string file_path = engine->getAbsolutePath(rel_path);
        std::string mime_type = "image/jpeg";

        const auto tmpPath = util::decrypt_file_to_temp(vault_id, rel_path, engine);

        FPDF_InitLibrary();
        FPDF_DOCUMENT doc = FPDF_LoadDocument(tmpPath.c_str(), nullptr);
        if (!doc) throw std::runtime_error("Failed to load PDF");

        FPDF_PAGE page = FPDF_LoadPage(doc, 0);
        if (!page) {
            FPDF_CloseDocument(doc);
            throw std::runtime_error("Failed to load first page");
        }

        int width = static_cast<int>(FPDF_GetPageWidth(page));
        int height = static_cast<int>(FPDF_GetPageHeight(page));

        int new_w = width, new_h = height;
        if (scale_it != params.end()) {
            float scale = std::stof(scale_it->second);
            new_w = static_cast<int>(width * scale);
            new_h = static_cast<int>(height * scale);
        } else if (size_it != params.end()) {
            int max_dim = std::stoi(size_it->second);
            float ratio = std::min(max_dim / static_cast<float>(width), max_dim / static_cast<float>(height));
            new_w = static_cast<int>(width * ratio);
            new_h = static_cast<int>(height * ratio);
        }

        FPDF_BITMAP bitmap = FPDFBitmap_Create(new_w, new_h, 0);
        FPDFBitmap_FillRect(bitmap, 0, 0, new_w, new_h, 0xFFFFFFFF);
        FPDF_RenderPageBitmap(bitmap, page, 0, 0, new_w, new_h, 0, 0);

        uint8_t* buffer = static_cast<uint8_t*>(FPDFBitmap_GetBuffer(bitmap));
        std::vector<uint8_t> rgb_data(new_w * new_h * 3);

        for (int y = 0; y < new_h; ++y) {
            for (int x = 0; x < new_w; ++x) {
                int idx = y * new_w + x;
                rgb_data[idx * 3 + 0] = buffer[idx * 4 + 2]; // R
                rgb_data[idx * 3 + 1] = buffer[idx * 4 + 1]; // G
                rgb_data[idx * 3 + 2] = buffer[idx * 4 + 0]; // B
            }
        }

        std::vector<uint8_t> jpeg_buf;
        util::compress_to_jpeg(rgb_data.data(), new_w, new_h, jpeg_buf);

        FPDFBitmap_Destroy(bitmap);
        FPDF_ClosePage(page);
        FPDF_CloseDocument(doc);
        FPDF_DestroyLibrary();

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

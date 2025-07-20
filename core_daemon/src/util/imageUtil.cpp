#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include "util/imageUtil.hpp"

#include <stb/stb_image.h>
#include <stb/stb_image_resize.h>
#include <turbojpeg.h>
#include <poppler/cpp/poppler-document.h>
#include <poppler/cpp/poppler-page.h>
#include <poppler/cpp/poppler-page-renderer.h>
#include <stdexcept>
#include <fstream>
#include <iomanip>
#include <iostream>

namespace vh::util {

void compress_to_jpeg(const uint8_t* rgb_data, const int width, const int height, std::vector<uint8_t>& out_buf,
                      const int quality) {
    tjhandle tj = tjInitCompress();
    if (!tj) throw std::runtime_error("Failed to initialize TurboJPEG compressor");

    unsigned char* jpeg_buf = nullptr;
    unsigned long jpeg_size = 0;

    int flags = 0; // You could use TJFLAG_FASTDCT | TJFLAG_FASTUPSAMPLE if you want speed

    if (tjCompress2(
            tj,
            rgb_data,
            width,
            0, // pitch (0 = auto)
            height,
            TJPF_RGB,
            &jpeg_buf,
            &jpeg_size,
            TJSAMP_444, // No chroma subsampling, best quality
            quality,
            flags) != 0) {
        std::string err = tjGetErrorStr();
        tjDestroy(tj);
        throw std::runtime_error("JPEG compression failed: " + err);
    }

    out_buf.assign(jpeg_buf, jpeg_buf + jpeg_size);
    tjFree(jpeg_buf);
    tjDestroy(tj);
}

std::vector<uint8_t> resize_and_compress_image(
    const std::string& path,
    const std::optional<std::string>& scale_opt,
    const std::optional<std::string>& size_opt) {

    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 3);
    if (!data) throw std::runtime_error("Failed to load image");

    int new_w = width, new_h = height;
    if (scale_opt) {
        const float scale = std::stof(*scale_opt);
        new_w = static_cast<int>(static_cast<float>(width) * scale);
        new_h = static_cast<int>(static_cast<float>(height) * scale);
    } else if (size_opt) {
        const int max_dim = std::stoi(*size_opt);
        const float ratio = std::min(static_cast<float>(max_dim) / static_cast<float>(width),
                                     static_cast<float>(max_dim) / static_cast<float>(height));
        new_w = static_cast<int>(static_cast<float>(width) * ratio);
        new_h = static_cast<int>(static_cast<float>(height) * ratio);
    }

    std::vector<uint8_t> resized(new_w * new_h * 3);
    stbir_resize_uint8(data, width, height, 0, resized.data(), new_w, new_h, 0, 3);
    stbi_image_free(data);

    // Compress to JPEG using libjpeg-turbo
    std::vector<uint8_t> compressed;
    compress_to_jpeg(resized.data(), new_w, new_h, compressed);
    return compressed;
}

std::vector<uint8_t> resize_and_compress_image_buffer(
    const uint8_t* data, size_t size,
    const std::optional<std::string>& scale_opt,
    const std::optional<std::string>& size_opt) {
    if (size < 4) {
        throw std::runtime_error("Buffer too small to be a valid image");
    }

    int width = 0, height = 0, channels = 0;
    unsigned char* decoded = stbi_load_from_memory(data, static_cast<int>(size), &width, &height, &channels, 3);
    if (!decoded) {
        const char* reason = stbi_failure_reason();
        throw std::runtime_error(
            std::string("Failed to decode image from memory: ") + (reason ? reason : "unknown error"));
    }

    int new_w = width, new_h = height;
    if (scale_opt) {
        float scale = std::stof(*scale_opt);
        new_w = static_cast<int>(width * scale);
        new_h = static_cast<int>(height * scale);
    } else if (size_opt) {
        int max_dim = std::stoi(*size_opt);
        float ratio = std::min(static_cast<float>(max_dim) / width, static_cast<float>(max_dim) / height);
        new_w = static_cast<int>(width * ratio);
        new_h = static_cast<int>(height * ratio);
    }

    std::vector<uint8_t> resized(new_w * new_h * 3);
    stbir_resize_uint8(decoded, width, height, 0, resized.data(), new_w, new_h, 0, 3);
    stbi_image_free(decoded);

    std::vector<uint8_t> compressed;
    compress_to_jpeg(resized.data(), new_w, new_h, compressed);
    return compressed;
}

std::vector<uint8_t> resize_and_compress_pdf_buffer(
    const uint8_t* data, size_t size,
    const std::optional<std::string>& scale_opt,
    const std::optional<std::string>& size_opt) {

    auto doc = poppler::document::load_from_raw_data(reinterpret_cast<const char*>(data), size);
    if (!doc || doc->is_locked()) {
        throw std::runtime_error("Failed to load or unlock PDF");
    }

    auto page = doc->create_page(0);
    if (!page) {
        throw std::runtime_error("Failed to load first page");
    }

    const double width = page->page_rect().width();
    const double height = page->page_rect().height();

    double scale = 1.0;
    if (scale_opt) {
        scale = std::stof(*scale_opt);
    } else if (size_opt) {
        int max_dim = std::stoi(*size_opt);
        double ratio = std::min(static_cast<double>(max_dim) / width, static_cast<double>(max_dim) / height);
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
    compress_to_jpeg(rgb.data(), w, h, jpeg_buf);

    return jpeg_buf;
}

void generateAndStoreThumbnail(const std::vector<uint8_t>& buffer, const std::filesystem::path& outputPath,
                               const std::string& mime, const unsigned int size) {
    std::vector<uint8_t> jpeg;

    if (mime.starts_with("image/")) {
        jpeg = resize_and_compress_image_buffer(
            buffer.data(), buffer.size(), std::nullopt, std::make_optional(std::to_string(size))
            );
    } else if (mime == "application/pdf") {
        jpeg = resize_and_compress_pdf_buffer(
            buffer.data(), buffer.size(), std::nullopt, std::make_optional(std::to_string(size))
            );
    } else {
        throw std::runtime_error("Unsupported MIME type for thumbnail generation: " + mime);
    }

    if (jpeg.empty()) {
        throw std::runtime_error("Thumbnail JPEG buffer is empty after processing");
    }

    std::filesystem::create_directories(outputPath.parent_path());
    std::ofstream out(outputPath, std::ios::binary);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to open thumbnail output path: " + outputPath.string());
    }

    out.write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
    if (!out.good()) {
        throw std::runtime_error("Failed to write thumbnail to disk: " + outputPath.string());
    }

    out.close();
}

}